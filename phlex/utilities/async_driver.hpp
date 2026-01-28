#ifndef PHLEX_UTILITIES_ASYNC_DRIVER_HPP
#define PHLEX_UTILITIES_ASYNC_DRIVER_HPP

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace phlex::experimental {

  template <typename RT>
  class async_driver {
    enum class states { off, drive, park };

  public:
    template <typename FT>
    async_driver(FT ft) : driver_{std::move(ft)}
    {
    }
    async_driver(void (*ft)(async_driver<RT>&)) : driver_{ft} {}

    std::optional<RT> operator()()
    {
      if (gear_ == states::off) {
        thread_ = std::jthread{[this] {
          try {
            gear_ = states::drive;
            driver_(*this);
          } catch (...) {
            cached_exception_ = std::current_exception();
          }
          gear_ = states::park;
          cv_.notify_one();
        }};
      } else {
        cv_.notify_one();
      }

      std::unique_lock lock{mutex_};
      cv_.wait(lock, [&] { return current_.has_value() or gear_ == states::park; });

      if (cached_exception_) {
        std::rethrow_exception(cached_exception_);
      }

      return std::exchange(current_, std::nullopt);
    }

    void stop()
    {
      // API that should only be called by the framework_graph
      gear_ = states::park;
      cv_.notify_one();
    }

    void yield(RT rt)
    {
      std::unique_lock lock{mutex_};
      current_ = std::make_optional(std::move(rt));
      cv_.notify_one();
      cv_.wait(lock);
      if (gear_ == states::park) {
        // Can only be in park at this point if the framework needs to prematurely shut down
        throw std::runtime_error("Framework shutdown");
      }
    }

  private:
    std::function<void(async_driver&)> driver_;
    std::optional<RT> current_;
    std::atomic<states> gear_ = states::off;
    std::jthread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::exception_ptr cached_exception_;
  };
}

#endif // PHLEX_UTILITIES_ASYNC_DRIVER_HPP

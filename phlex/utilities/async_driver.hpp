#ifndef PHLEX_UTILITIES_ASYNC_DRIVER_HPP
#define PHLEX_UTILITIES_ASYNC_DRIVER_HPP

#include <atomic>
#include <condition_variable>
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

    ~async_driver() { thread_.join(); }

    std::optional<RT> operator()()
    {
      if (gear_ == states::off) {
        thread_ = std::thread{[this] {
          driver_(*this);
          gear_ = states::park;
          cv_.notify_one();
        }};
        gear_ = states::drive;
      } else {
        cv_.notify_one();
      }

      std::unique_lock lock{mutex_};
      cv_.wait(lock, [&] { return current_.has_value() or gear_ == states::park; });
      return std::exchange(current_, std::nullopt);
    }

    void yield(RT rt)
    {
      std::unique_lock lock{mutex_};
      current_ = std::make_optional(std::move(rt));
      cv_.notify_one();
      cv_.wait(lock);
    }

  private:
    std::function<void(async_driver&)> driver_;
    std::optional<RT> current_;
    std::atomic<states> gear_ = states::off;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
  };
}

#endif // PHLEX_UTILITIES_ASYNC_DRIVER_HPP

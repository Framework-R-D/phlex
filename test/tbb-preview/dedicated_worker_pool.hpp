#ifndef TEST_TBB_PREVIEW_DEDICATED_WORKER_POOL_HPP
#define TEST_TBB_PREVIEW_DEDICATED_WORKER_POOL_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <semaphore>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace phlex::test {

  template <typename Factory>
  class managed_thread {
  public:
    explicit managed_thread(Factory make_worker);
    managed_thread(managed_thread const&) = delete;
    managed_thread(managed_thread&&) = delete;
    managed_thread& operator=(managed_thread const&) = delete;
    managed_thread& operator=(managed_thread&&) = delete;

    ~managed_thread();

    // Callers must serialize access to this handle and finish before destruction.
    template <typename Operation>
    auto execute(Operation operation);

  private:
    using operation_type = std::function<void(std::invoke_result_t<Factory&>&)>;

    void wait();
    void run();

    Factory make_worker_;
    operation_type operation_;
    std::binary_semaphore work_ready_{0};
    std::binary_semaphore slot_ready_{0};
    std::atomic<bool> stopping_{false};
    std::exception_ptr exception_;
    std::jthread thread_;
  };

  template <typename Factory>
  class dedicated_worker_pool {
    using worker_type = managed_thread<Factory>;
    std::vector<std::unique_ptr<worker_type>> workers_;

  public:
    using handle_type = worker_type*;

    // The factory returns owned state with non-throwing destruction. Each worker
    // receives its own factory copy; shared captures remain the caller's responsibility.
    dedicated_worker_pool(Factory const& make_worker,
                          std::size_t const num_workers,
                          std::source_location const location = std::source_location::current());

    // Handles are non-owning and must not outlive the pool.
    auto handles() const;
  };

  template <typename Factory>
  managed_thread<Factory>::managed_thread(Factory make_worker) :
    make_worker_{std::move(make_worker)}, thread_{[this] { run(); }}
  {
    slot_ready_.acquire();
    if (exception_) {
      // jthread joins the failed worker during constructor unwinding.
      std::rethrow_exception(exception_);
    }
  }

  template <typename Factory>
  managed_thread<Factory>::~managed_thread()
  {
    stopping_ = true;
    work_ready_.release();
  }

  template <typename Factory>
  template <typename Operation>
  auto managed_thread<Factory>::execute(Operation operation)
  {
    // The caller remains blocked, so even move-only operations can be borrowed.
    using result_type = std::invoke_result_t<Operation&, std::invoke_result_t<Factory&>&>;
    static_assert(!std::is_reference_v<result_type>, "Worker results must be returned by value.");
    if constexpr (std::is_void_v<result_type>) {
      operation_ = [&operation](auto& worker) { std::invoke(operation, worker); };
      work_ready_.release();
      wait();
    } else {
      std::optional<result_type> result;
      operation_ = [&result, &operation](auto& worker) {
        result.emplace(std::invoke(operation, worker));
      };
      work_ready_.release();
      wait();
      // NOLINTNEXTLINE(bugprone-unchecked-optional-access): The worker always sets the result.
      return std::move(*result);
    }
  }

  template <typename Factory>
  void managed_thread<Factory>::wait()
  {
    slot_ready_.acquire();
    operation_ = nullptr;
    if (exception_) {
      std::rethrow_exception(std::exchange(exception_, nullptr));
    }
  }

  template <typename Factory>
  void managed_thread<Factory>::run()
  {
    try {
      // Worker state lives entirely on this thread, including destruction.
      auto worker = std::invoke(make_worker_);
      slot_ready_.release();
      while (true) {
        work_ready_.acquire();
        if (stopping_) {
          break;
        }

        try {
          operation_(worker);
        } catch (...) {
          exception_ = std::current_exception();
        }
        slot_ready_.release();
      }
    } catch (...) {
      exception_ = std::current_exception();
      slot_ready_.release();
    }
  }

  template <typename Factory>
  dedicated_worker_pool<Factory>::dedicated_worker_pool(Factory const& make_worker,
                                                        std::size_t const num_workers,
                                                        std::source_location const location)
  {
    if (num_workers == 0) {
      throw std::invalid_argument{"A dedicated worker pool requires at least one worker"};
    }
    workers_.reserve(num_workers);
    for (std::size_t i = 0; i < num_workers; ++i) {
      spdlog::info("Creating managed thread {} (pool created at line {})", i, location.line());
      workers_.push_back(std::make_unique<worker_type>(make_worker));
    }
  }

  template <typename Factory>
  auto dedicated_worker_pool<Factory>::handles() const
  {
    return workers_ | std::views::transform([](auto const& worker) { return worker.get(); });
  }

} // namespace phlex::test

#endif // TEST_TBB_PREVIEW_DEDICATED_WORKER_POOL_HPP

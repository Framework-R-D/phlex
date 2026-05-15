#ifndef PHLEX_UTILITIES_ASYNC_DRIVER_HPP
#define PHLEX_UTILITIES_ASYNC_DRIVER_HPP

// ===========================================================================================
// async_driver mediates between a TBB task thread and a dedicated driver thread that produces
// items one at a time via yield().
//
// The two threads alternate ownership of a single slot using two binary semaphores:
//
//   item_ready_  (driver → TBB):  the driver releases this after placing an item in current_
//                                 (or after the driver function returns).  The TBB thread
//                                 acquires it before reading current_.
//
//   slot_ready_  (TBB → driver):  the TBB thread releases this after consuming current_.
//                                 The driver acquires it inside yield() before overwriting
//                                 current_ with the next item.
//
// Because each semaphore starts at 0 and is released exactly once per cycle, neither thread
// can advance past its semaphore until the other thread is ready.  This strict alternation
// avoids any need for a mutex or additional buffering.
// ===========================================================================================

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>

namespace phlex::experimental {

  template <typename RT>
  class async_driver {
    enum class states : std::uint8_t { off, drive, park };

  public:
    template <typename FT>
    explicit async_driver(FT ft) : driver_{std::move(ft)}
    {
    }
    explicit async_driver(void (*ft)(async_driver<RT>&)) : driver_{ft} {}

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
          item_ready_.release();
        }};
      } else {
        slot_ready_.release();
      }

      item_ready_.acquire();

      if (cached_exception_) {
        std::rethrow_exception(cached_exception_);
      }

      return std::exchange(current_, std::nullopt);
    }

    void stop()
    {
      // API that should only be called by the framework_graph
      gear_ = states::park;
      slot_ready_.release();
    }

    void yield(RT rt)
    {
      current_ = std::make_optional(std::move(rt));

      item_ready_.release();
      slot_ready_.acquire();

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
    std::binary_semaphore item_ready_{0};
    std::binary_semaphore slot_ready_{0};
    std::exception_ptr cached_exception_;
  };
}

#endif // PHLEX_UTILITIES_ASYNC_DRIVER_HPP

#include "catch2/catch_test_macros.hpp"
#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <tuple>

using namespace oneapi::tbb;

namespace {

  template <typename F, typename... Args>
  class managed_thread {
    enum class states : std::uint8_t { off, drive, park };

    F f_;
    std::optional<std::tuple<Args...>> args_;
    std::jthread thread_;
    std::binary_semaphore work_ready_{0};
    std::binary_semaphore slot_ready_{0};
    std::atomic<states> gear_ = states::off;
    std::exception_ptr exception_;

    void run(std::stop_token st)
    {
      while (true) {
        work_ready_.acquire();
        if (gear_ == states::park || st.stop_requested()) {
          break;
        }

        try {
          std::apply(f_, args_.value());
        } catch (...) {
          exception_ = std::current_exception();
        }
        slot_ready_.release();
      }
    }

  public:
    explicit managed_thread(F f) :
      f_{std::move(f)}, thread_{[this](std::stop_token st) { run(st); }}
    {
    }
    managed_thread(managed_thread const&) = delete;
    managed_thread(managed_thread&&) = delete;
    managed_thread& operator=(managed_thread const&) = delete;
    managed_thread& operator=(managed_thread&&) = delete;

    ~managed_thread()
    {
      gear_ = states::park;
      work_ready_.release();
    }

    void execute(Args const&... args)
    {
      if (gear_ == states::off) {
        gear_ = states::drive;
      }

      args_ = std::make_tuple(args...);
      work_ready_.release();
      slot_ready_.acquire();
      args_.reset();

      if (exception_) {
        std::rethrow_exception(std::exchange(exception_, nullptr));
      }
    }
  };

} // namespace

TEST_CASE("std::threads as limited resources")
{
  auto f = [](unsigned int const i) {
    spdlog::info("Executing on thread {} with argument {}", std::this_thread::get_id(), i);
  };
  using managed_thread_type = managed_thread<decltype(f), unsigned int>;

  managed_thread_type t1{f};
  managed_thread_type t2{f};
  flow::resource_limiter<managed_thread_type*> pinned_thread_resource{&t1, &t2};

  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) {
                         if (i < 10) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  flow::resource_limited_node<unsigned int, std::tuple<>> geant4_node{
    g,
    flow::unlimited,
    std::tie(pinned_thread_resource),
    [](unsigned int const i, auto&, managed_thread_type* geant4_resource) {
      spdlog::info("Launching from thread {} with argument {}", std::this_thread::get_id(), i);
      geant4_resource->execute(i);
    }};

  make_edge(src, geant4_node);

  src.activate();
  g.wait_for_all();
}

TEST_CASE("Throw exception from managed thread", "[multithreading]")
{
  auto f = [](unsigned int const i) {
    spdlog::info("Executing on thread {} with argument {}", std::this_thread::get_id(), i);
    if (i == 5) {
      throw std::runtime_error{"Exception from managed thread"};
    }
  };
  using managed_thread_type = managed_thread<decltype(f), unsigned int>;

  managed_thread_type t1{f};
  managed_thread_type t2{f};
  flow::resource_limiter<managed_thread_type*> pinned_thread_resource{&t1, &t2};

  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) {
                         if (i < 10) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  flow::resource_limited_node<unsigned int, std::tuple<>> geant4_node{
    g,
    flow::unlimited,
    std::tie(pinned_thread_resource),
    [](unsigned int const i, auto&, managed_thread_type* geant4_resource) {
      spdlog::info("Launching from thread {} with argument {}", std::this_thread::get_id(), i);
      try {
        geant4_resource->execute(i);
      } catch (...) {
        // Ignore exceptions from the managed thread
      }
    }};

  make_edge(src, geant4_node);

  src.activate();
  g.wait_for_all();
}

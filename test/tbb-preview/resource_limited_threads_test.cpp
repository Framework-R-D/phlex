#include <boost/callable_traits/args.hpp>
#include <boost/mp11/list.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
// The format library does not natively support std::thread::id (IWYU does not know this)
#include <fmt/std.h> // IWYU pragma: keep
#include <oneapi/tbb/flow_graph.h>
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
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace oneapi::tbb;

namespace {

  template <typename Factory, typename F, typename Args>
  class managed_thread;

  template <typename Factory, typename F, typename... Args>
  class managed_thread<Factory, F, std::tuple<Args...>> {
  public:
    managed_thread(Factory make_worker, F f) :
      make_worker_{std::move(make_worker)}, f_{std::move(f)}, thread_{[this] { run(); }}
    {
      slot_ready_.acquire();
      if (exception_) {
        // jthread joins the failed worker during constructor unwinding.
        std::rethrow_exception(exception_);
      }
    }
    managed_thread(managed_thread const&) = delete;
    managed_thread(managed_thread&&) = delete;
    managed_thread& operator=(managed_thread const&) = delete;
    managed_thread& operator=(managed_thread&&) = delete;

    ~managed_thread()
    {
      stopping_ = true;
      work_ready_.release();
    }

    // Callers must serialize access to this handle and finish before destruction.
    void execute(std::decay_t<Args> const&... args)
    {
      args_ = std::make_tuple(args...);
      work_ready_.release();
      slot_ready_.acquire();
      args_.reset();

      if (exception_) {
        std::rethrow_exception(std::exchange(exception_, nullptr));
      }
    }

  private:
    void run()
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
            // The args_ value is guaranteed to be set by the caller.
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            std::apply([&](auto&... args) { std::invoke(f_, worker, args...); }, args_.value());
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

    Factory make_worker_;
    F f_;
    std::optional<std::tuple<std::decay_t<Args>...>> args_;
    std::binary_semaphore work_ready_{0};
    std::binary_semaphore slot_ready_{0};
    std::atomic<bool> stopping_{false};
    std::exception_ptr exception_;
    std::jthread thread_;
  };

  template <typename Factory, typename F>
  class dedicated_worker_pool {
    using worker_type =
      managed_thread<Factory, F, boost::mp11::mp_pop_front<boost::callable_traits::args_t<F>>>;
    std::vector<std::unique_ptr<worker_type>> workers_;

  public:
    using handle_type = worker_type*;

    // The factory returns owned state with non-throwing destruction. Each worker
    // receives its own callable copies; shared captures remain the caller's responsibility.
    dedicated_worker_pool(Factory make_worker,
                          F f,
                          std::size_t const num_workers,
                          std::source_location const location = std::source_location::current())
    {
      if (num_workers == 0) {
        throw std::invalid_argument{"A dedicated worker pool requires at least one worker"};
      }
      workers_.reserve(num_workers);
      for (std::size_t i = 0; i < num_workers; ++i) {
        spdlog::info("Creating managed thread {} (pool created at line {})", i, location.line());
        workers_.push_back(std::make_unique<worker_type>(make_worker, f));
      }
    }

    // Handles are non-owning and must not outlive the pool.
    auto handles() const
    {
      return workers_ | std::views::transform([](auto const& worker) { return worker.get(); });
    }
  };

  struct worker_lifecycle {
    std::thread::id initialized;
    std::thread::id executed;
    std::thread::id destroyed;
    unsigned int calls{};
  };

  class tracked_worker {
  public:
    worker_lifecycle& lifecycle;

    explicit tracked_worker(worker_lifecycle& record) : lifecycle{record}
    {
      lifecycle.initialized = std::this_thread::get_id();
    }
    tracked_worker(tracked_worker const&) = delete;
    tracked_worker(tracked_worker&&) = delete;
    tracked_worker& operator=(tracked_worker const&) = delete;
    tracked_worker& operator=(tracked_worker&&) = delete;
    ~tracked_worker() { lifecycle.destroyed = std::this_thread::get_id(); }
  };

} // namespace

TEST_CASE("std::threads as limited resources")
{
  std::atomic<unsigned int> calls{};
  auto make_worker = [] { return 0u; };
  auto f = [&calls](unsigned int& worker_calls, unsigned int const i) {
    spdlog::info("Executing on thread {} with argument {}", std::this_thread::get_id(), i);
    ++calls;
    ++worker_calls;
  };

  std::size_t const num_workers = 2;
  dedicated_worker_pool const pinned_work{make_worker, f, num_workers};
  flow::resource_limiter<decltype(pinned_work)::handle_type> pinned_work_resource{
    pinned_work.handles()};

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
    std::tie(pinned_work_resource),
    [](unsigned int const i, auto&, decltype(pinned_work)::handle_type geant4_resource) {
      spdlog::info("Launching from thread {} with argument {}", std::this_thread::get_id(), i);
      geant4_resource->execute(i);
    }};

  make_edge(src, geant4_node);

  src.activate();
  g.wait_for_all();
  CHECK(calls.load() == 10);
}

TEST_CASE("Throw exception from managed thread", "[multithreading]")
{
  std::atomic<unsigned int> exceptions{};
  auto make_worker = [] { return 0u; };
  auto f = [](unsigned int&, unsigned int const i) {
    spdlog::info("Executing on thread {} with argument {}", std::this_thread::get_id(), i);
    if (i == 5) {
      throw std::runtime_error{"Exception from managed thread"};
    }
  };
  dedicated_worker_pool const pinned_work{make_worker, f, 2};
  flow::resource_limiter<decltype(pinned_work)::handle_type> pinned_work_resource{
    pinned_work.handles()};

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
    std::tie(pinned_work_resource),
    [&exceptions](unsigned int const i, auto&, decltype(pinned_work)::handle_type geant4_resource) {
      spdlog::info("Launching from thread {} with argument {}", std::this_thread::get_id(), i);
      try {
        geant4_resource->execute(i);
      } catch (...) {
        ++exceptions;
      }
    }};

  make_edge(src, geant4_node);

  src.activate();
  g.wait_for_all();
  CHECK(exceptions.load() == 1);
}

TEST_CASE("Dedicated worker lifecycle preserves thread affinity", "[multithreading]")
{
  std::vector<worker_lifecycle> records(2);
  std::size_t initialized{};
  auto make_worker = [&] { return tracked_worker{records.at(initialized++)}; };
  unsigned int result{};
  auto f = [&](tracked_worker& worker, unsigned int const& i, unsigned int const j) {
    worker.lifecycle.executed = std::this_thread::get_id();
    ++worker.lifecycle.calls;
    result = i + j;
    if (i == 0) {
      throw std::runtime_error{"Exception from dedicated worker"};
    }
  };

  CHECK_THROWS_AS((dedicated_worker_pool{make_worker, f, 0}), std::invalid_argument);
  CHECK(initialized == 0);

  {
    dedicated_worker_pool const work{make_worker, f, 2};
    REQUIRE(initialized == 2);
    REQUIRE(std::ranges::size(work.handles()) == 2);
    std::size_t index{};
    for (auto* handle : work.handles()) {
      auto const& record = records[index++];
      CHECK(record.initialized != std::this_thread::get_id());
      CHECK(record.destroyed == std::thread::id{});
      handle->execute(1u, 2u);
      CHECK(result == 3);
      CHECK_THROWS_AS(handle->execute(0u, 2u), std::runtime_error);
      handle->execute(3u, 4u);
      CHECK(result == 7);
      CHECK(record.executed == record.initialized);
      CHECK(record.calls == 3);
    }
    CHECK(records[0].initialized != records[1].initialized);
  }
  for (auto const& record : records) {
    CHECK(record.destroyed == record.initialized);
  }
}

TEST_CASE("Dedicated worker startup failure cleans up on worker threads", "[multithreading]")
{
  auto const fail_at = GENERATE(0uz, 1uz);
  CAPTURE(fail_at);

  std::vector<worker_lifecycle> records(2);
  std::size_t attempted{};
  auto make_worker = [&] {
    auto const index = attempted++;
    auto worker = std::make_unique<tracked_worker>(records.at(index));
    if (index == fail_at) {
      throw std::runtime_error{"Worker initialization failed"};
    }
    return worker;
  };
  auto f = [](std::unique_ptr<tracked_worker>&) {};

  CHECK_THROWS_AS((dedicated_worker_pool{make_worker, f, 2}), std::runtime_error);
  CHECK(attempted == fail_at + 1);
  for (std::size_t i = 0; i < attempted; ++i) {
    CHECK(records[i].initialized != std::this_thread::get_id());
    CHECK(records[i].destroyed == records[i].initialized);
    CHECK(records[i].calls == 0);
  }
  for (std::size_t i = attempted; i < records.size(); ++i) {
    CHECK(records[i].initialized == std::thread::id{});
  }
}

TEST_CASE("Idle dedicated workers are destroyed on their threads", "[multithreading]")
{
  worker_lifecycle record;
  auto make_worker = [&] { return tracked_worker{record}; };
  auto f = [](tracked_worker&) {};
  {
    dedicated_worker_pool const work{make_worker, f, 1};
    CHECK(record.initialized != std::thread::id{});
    CHECK(record.destroyed == std::thread::id{});
  }
  CHECK(record.initialized != std::this_thread::get_id());
  CHECK(record.destroyed == record.initialized);
  CHECK(record.calls == 0);
}

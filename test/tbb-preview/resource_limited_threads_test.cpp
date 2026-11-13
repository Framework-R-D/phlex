#include "phlex/core/framework_graph.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource/concepts.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/model/fwd.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
// The format library does not natively support std::thread::id (IWYU does not know this)
#include <fmt/std.h> // IWYU pragma: keep
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
#include <span>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

  template <typename Factory>
  class managed_thread {
  public:
    explicit managed_thread(Factory make_worker) :
      make_worker_{std::move(make_worker)}, thread_{[this] { run(); }}
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
    template <typename Operation>
    auto execute(Operation operation)
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

  private:
    using operation_type = std::function<void(std::invoke_result_t<Factory&>&)>;

    void wait()
    {
      slot_ready_.acquire();
      operation_ = nullptr;
      if (exception_) {
        std::rethrow_exception(std::exchange(exception_, nullptr));
      }
    }

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
                          std::source_location const location = std::source_location::current())
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

  struct simulation_result {
    unsigned int value;
    std::thread::id worker_thread;
  };

  // Algorithms use typed operations; dispatch and worker access stay inside the resource.
  class mock_simulation {
    using pool_type = dedicated_worker_pool<std::function<tracked_worker()>>;

  public:
    class token_type {
    public:
      explicit token_type(pool_type::handle_type thread) : thread_{thread} {}

      simulation_result process_event(unsigned int value) const
      {
        return thread_->execute([value](tracked_worker& worker) {
          auto& record = worker.lifecycle;
          record.executed = std::this_thread::get_id();
          ++record.calls;
          return simulation_result{.value = value + 1, .worker_thread = record.executed};
        });
      }

      void fail() const
      {
        thread_->execute([](tracked_worker& worker) {
          worker.lifecycle.executed = std::this_thread::get_id();
          throw std::runtime_error{"Exception from managed thread"};
        });
      }

    private:
      pool_type::handle_type thread_;
    };

    explicit mock_simulation(std::vector<worker_lifecycle>& records) :
      workers_{[&records, next = std::make_shared<std::size_t>(0)]() mutable {
                 return tracked_worker{records.at((*next)++)};
               },
               records.size()}
    {
      for (auto* thread : workers_.handles()) {
        tokens_.emplace_back(thread);
      }
    }

    std::span<token_type const> tokens() const { return tokens_; }

  private:
    pool_type workers_;
    std::vector<token_type> tokens_;
  };

  struct ordinary_pool {
    static std::vector<int> tokens() { return {10, 20}; }
  };

  struct ordinary_unlimited {};
  struct ordinary_serialized {
    using token_type = ordinary_serialized const*;
  };

  void add_events(phlex::detail::framework_graph& graph,
                  unsigned int num_runs = 2,
                  unsigned int events_per_run = 10)
  {
    using namespace phlex;
    graph.add_driver(
      detail::driver_bundle{.driver =
                              [num_runs, events_per_run](detail::framework_driver& driver) {
                                auto const job = data_cell_index::job();
                                driver.yield(job);
                                for (unsigned int r = 0; r < num_runs; ++r) {
                                  auto const run = job->make_child("run", r);
                                  driver.yield(run);
                                  for (unsigned int e = 0; e < events_per_run; ++e) {
                                    driver.yield(run->make_child("event", e));
                                  }
                                }
                              },
                            .hierarchy = fixed_hierarchy{{"run", "event"}}});
    graph
      .provide("number",
               [](data_cell_index const& index) -> unsigned int { return index.number(); })
      .output_product("input", "number", "event");
  }

  static_assert(phlex::detail::pooled_resource<mock_simulation>);

} // namespace

TEST_CASE("Algorithms call pinned resource methods across runs", "[resource][affine]")
{
  using namespace phlex;
  auto const parallelism = GENERATE(1, 4);
  std::vector<worker_lifecycle> records(2);
  std::atomic<unsigned int> observed{};
  std::atomic<unsigned int> sum{};
  std::atomic<bool> wrong_thread{};
  {
    auto graph = detail::framework_graph::without_driver("test", parallelism);
    graph.add_pooled_resource<mock_simulation>(records);
    graph.add_pooled_resource<ordinary_pool>();
    graph.add_unlimited_resource<ordinary_unlimited>();
    graph.add_serialized_resource<ordinary_serialized>();
    add_events(graph);
    graph
      .transform(
        "simulate",
        [&wrong_thread](unsigned int value,
                        int token,
                        ordinary_unlimited const*,
                        mock_simulation::token_type sim) {
          auto const caller = std::this_thread::get_id();
          auto result = sim.process_event(value);
          if (result.worker_thread == caller || std::this_thread::get_id() != caller ||
              (token != 10 && token != 20)) {
            wrong_thread = true;
          }
          return result.value;
        },
        concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"},
                    resource<ordinary_pool>{},
                    resource<ordinary_unlimited>{},
                    resource<mock_simulation>{})
      .output_product_suffixes("result");
    graph
      .observe(
        "consume",
        [&observed, &sum, &wrong_thread](
          unsigned int value, mock_simulation::token_type sim, ordinary_serialized const*) {
          auto const caller = std::this_thread::get_id();
          auto result = sim.process_event(value);
          if (result.worker_thread == caller || std::this_thread::get_id() != caller) {
            wrong_thread = true;
          }
          ++observed;
          sum += value;
        },
        concurrency::unlimited)
      .input_family(product_selector{.creator = "simulate", .suffix = "result"},
                    resource<mock_simulation>{},
                    resource<ordinary_serialized>{});
    graph.execute();
    CHECK(graph.execution_count("simulate") == 20);
    CHECK(graph.execution_count("consume") == 20);
    CHECK(graph.seen_cell_count("/job/run") == 2);
  }
  CHECK(observed == 20);
  CHECK(sum == 110);
  CHECK_FALSE(wrong_thread);
  CHECK(records[0].calls + records[1].calls == 40);
  for (auto const& record : records) {
    CHECK(record.initialized != std::this_thread::get_id());
    CHECK(record.destroyed == record.initialized);
    if (record.calls != 0) {
      CHECK(record.executed == record.initialized);
    }
  }
}

TEST_CASE("Resource method exceptions propagate through ordinary algorithms", "[resource][affine]")
{
  using namespace phlex;
  std::vector<worker_lifecycle> records(2);
  std::atomic<bool> wrong_thread{};
  {
    auto graph = detail::framework_graph::without_driver("test", 4);
    graph.add_pooled_resource<mock_simulation>(records);
    // Test exception delivery without queued-request cancellation in the TBB preview limiter.
    add_events(graph, 1, 1);
    graph
      .transform(
        "fail",
        [&wrong_thread](unsigned int, mock_simulation::token_type sim) -> unsigned int {
          auto const caller = std::this_thread::get_id();
          try {
            sim.fail();
          } catch (...) {
            auto result = sim.process_event(0);
            if (result.worker_thread == caller || std::this_thread::get_id() != caller) {
              wrong_thread = true;
            }
            throw;
          }
          return 0;
        },
        concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"},
                    resource<mock_simulation>{})
      .output_product_suffixes("result");
    graph.observe("consume", [](unsigned int) {})
      .input_family(product_selector{.creator = "fail", .suffix = "result"});
    CHECK_THROWS_WITH(graph.execute(), "Exception from managed thread");
    CHECK(graph.execution_count("fail") == 0);
    CHECK(graph.execution_count("consume") == 0);
  }
  CHECK_FALSE(wrong_thread);
  for (auto const& record : records) {
    CHECK(record.destroyed == record.initialized);
  }
}

TEST_CASE("Only resource operations leave the calling node body", "[resource][affine]")
{
  std::vector<worker_lifecycle> records(1);
  mock_simulation const resource{records};
  auto token = resource.tokens().front();
  auto const caller = std::this_thread::get_id();
  std::thread::id before_algorithm;
  std::thread::id algorithm;
  std::thread::id after_algorithm;
  auto operation = [&algorithm](unsigned int value, mock_simulation::token_type sim) {
    algorithm = std::this_thread::get_id();
    auto result = sim.process_event(value);
    return std::make_unique<unsigned int>(result.value);
  };
  auto body = [&before_algorithm, &after_algorithm](
                auto const& ft, unsigned int value, mock_simulation::token_type sim) {
    before_algorithm = std::this_thread::get_id();
    auto result = std::invoke(ft, value, sim);
    after_algorithm = std::this_thread::get_id();
    return result;
  };
  auto result = phlex::detail::invoke_with_resources<mock_simulation>(
    body, std::forward_as_tuple(operation, 42), std::tie(token), std::tuple<>{});
  REQUIRE(result);
  CHECK(*result == 43);
  CHECK(before_algorithm == caller);
  CHECK(algorithm == caller);
  CHECK(after_algorithm == caller);
  CHECK(records[0].executed == records[0].initialized);
  CHECK(records[0].executed != caller);
}

TEST_CASE("Managed thread returns detached values without futures", "[multithreading]")
{
  managed_thread thread{[] { return 42; }};
  CHECK(thread.execute([](int& worker) { return worker + 1; }) == 43);
  auto result = thread.execute([](int& worker) { return std::make_unique<int>(worker); });
  REQUIRE(result);
  CHECK(*result == 42);
  CHECK_THROWS_WITH(thread.execute([](int&) -> int { throw std::runtime_error{"return failed"}; }),
                    "return failed");
  CHECK(thread.execute([](int& worker) { return worker; }) == 42);
}

TEST_CASE("Dedicated worker lifecycle preserves thread affinity", "[multithreading]")
{
  std::vector<worker_lifecycle> records(2);
  std::size_t initialized{};
  auto make_worker = [&records, &initialized] { return tracked_worker{records.at(initialized++)}; };
  unsigned int result{};
  auto f = [&result](tracked_worker& worker, unsigned int const& i, unsigned int const j) {
    worker.lifecycle.executed = std::this_thread::get_id();
    ++worker.lifecycle.calls;
    result = i + j;
    if (i == 0) {
      throw std::runtime_error{"Exception from dedicated worker"};
    }
  };

  CHECK_THROWS_AS((dedicated_worker_pool{make_worker, 0}), std::invalid_argument);
  CHECK(initialized == 0);

  {
    dedicated_worker_pool const work{make_worker, 2};
    REQUIRE(initialized == 2);
    REQUIRE(std::ranges::size(work.handles()) == 2);
    std::size_t index{};
    for (auto* handle : work.handles()) {
      auto const& record = records[index++];
      CHECK(record.initialized != std::this_thread::get_id());
      CHECK(record.destroyed == std::thread::id{});
      handle->execute([&f](auto& worker) { f(worker, 1u, 2u); });
      CHECK(result == 3);
      CHECK_THROWS_AS(handle->execute([&f](auto& worker) { f(worker, 0u, 2u); }),
                      std::runtime_error);
      handle->execute(
        [&f, value = std::make_unique<unsigned int>(3u)](auto& worker) { f(worker, *value, 4u); });
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
  auto make_worker = [fail_at, &attempted, &records] {
    auto const index = attempted++;
    auto worker = std::make_unique<tracked_worker>(records.at(index));
    if (index == fail_at) {
      throw std::runtime_error{"Worker initialization failed"};
    }
    return worker;
  };
  CHECK_THROWS_AS((dedicated_worker_pool{make_worker, 2}), std::runtime_error);
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
  auto make_worker = [&record] { return tracked_worker{record}; };
  {
    dedicated_worker_pool const work{make_worker, 1};
    CHECK(record.initialized != std::thread::id{});
    CHECK(record.destroyed == std::thread::id{});
  }
  CHECK(record.initialized != std::this_thread::get_id());
  CHECK(record.destroyed == record.initialized);
  CHECK(record.calls == 0);
}

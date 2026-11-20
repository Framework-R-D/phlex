#include "phlex/core/framework_graph.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/model/fwd.hpp"
#include "test/tbb-preview/dedicated_worker_pool.hpp"
#include "test/tbb-preview/mock_simulation.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
// The format library does not natively support std::thread::id (IWYU does not know this)
#include <fmt/std.h> // IWYU pragma: keep

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>

using phlex::test::dedicated_worker_pool;
using phlex::test::managed_thread;
using phlex::test::mock_simulation;
using phlex::test::run_context;
using phlex::test::tracked_worker;
using phlex::test::worker_lifecycle;

namespace {

  struct ordinary_pool {
    static std::vector<int> tokens() { return {10, 20}; }
  };

  struct ordinary_unlimited {};
  struct ordinary_serialized {
    using token_type = ordinary_serialized const*;
  };

  void add_events(phlex::detail::framework_graph& graph,
                  unsigned int const num_runs,
                  unsigned int const events_per_run)
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
    add_events(graph, 2, 10);
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

TEST_CASE("Pinned resource methods consume run context and produce run summaries",
          "[resource][affine][fold]")
{
  using namespace phlex;
  auto const parallelism = GENERATE(1, 4);
  std::vector<worker_lifecycle> records(2);
  std::vector<unsigned int> summaries(2);
  std::atomic<bool> wrong_thread{};
  {
    auto graph = detail::framework_graph::without_driver("test", parallelism);
    graph.add_pooled_resource<mock_simulation>(records);
    add_events(graph, 2, 10);

    // Begin-run-like setup is data, repeated by the join for each event in that run.
    graph
      .provide("run_context",
               [](data_cell_index const& index) {
                 return run_context{.number = index.number(),
                                    .offset =
                                      static_cast<unsigned int>((index.number() + 1) * 100)};
               })
      .output_product("input", "context", "run");
    graph
      .transform(
        "simulate",
        [&wrong_thread](
          unsigned int value, run_context const& run, mock_simulation::token_type sim) {
          auto const caller = std::this_thread::get_id();
          auto const result = sim.process_event(value, run);
          if (result.worker_thread == caller || std::this_thread::get_id() != caller) {
            wrong_thread = true;
          }
          return result.value;
        },
        concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"},
                    product_selector{.creator = "input", .layer = "run", .suffix = "context"},
                    resource<mock_simulation>{})
      .output_product_suffixes("result");

    graph
      .fold(
        "run_sum",
        [](std::atomic<unsigned int>& sum, unsigned int value) { sum += value; },
        concurrency::unlimited,
        "run")
      .input_family(product_selector{.creator = "simulate", .layer = "event", .suffix = "result"})
      .output_product_suffixes("sum");
    // The completed fold supplies end-run-like results, not a global worker barrier.
    graph
      .observe(
        "report_run",
        [&summaries](unsigned int sum, run_context const& run) { summaries.at(run.number) = sum; },
        concurrency::serial)
      .input_family(product_selector{.creator = "run_sum", .layer = "run", .suffix = "sum"},
                    product_selector{.creator = "input", .layer = "run", .suffix = "context"});

    graph.execute();
    CHECK(graph.execution_count("run_context") == 2);
    CHECK(graph.execution_count("simulate") == 20);
    CHECK(graph.execution_count("run_sum") == 20);
    CHECK(graph.execution_count("report_run") == 2);
  }
  // Each run has event values 0..9: sum(value + 1 + offset) = 55 + 10 * offset.
  CHECK(summaries == std::vector<unsigned int>{1055, 2055});
  CHECK_FALSE(wrong_thread);
  CHECK(records[0].calls + records[1].calls == 20);
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

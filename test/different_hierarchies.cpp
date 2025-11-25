// =======================================================================================
// This test executes the following graph
//
//        Multiplexer
//        |         |
//    job_add(*) run_add(^)
//        |         |
//        |     verify_run_sum
//        |
//   verify_job_sum
//
// where the asterisk (*) indicates a fold step over the full job, and the caret (^)
// represents a fold step over each run.
//
// The hierarchy tested is:
//
//    job
//     │
//     ├ trigger primitive
//     │
//     └ run
//        │
//        └ event
//
// As the run_add node performs folds only over "runs", any "trigger primitive"
// stores are excluded from the fold result.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <ranges>
#include <string>

using namespace phlex::experimental;

namespace {
  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  // job -> run -> event levels
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  // job -> trigger primitive levels
  constexpr auto primitive_limit = 10u;

  void levels_to_process(framework_driver& driver)
  {
    auto job_store = product_store::base();
    driver.yield(job_store);

    // job -> run -> event levels
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_store = job_store->make_child(i, "run");
      driver.yield(run_store);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        auto event_store = run_store->make_child(j, "event");
        event_store->add_product("number", j);
        driver.yield(event_store);
      }
    }

    // job -> trigger primitive levels
    for (unsigned i : std::views::iota(0u, primitive_limit)) {
      auto tp_store = job_store->make_child(i, "trigger primitive");
      tp_store->add_product("number", i);
      driver.yield(tp_store);
    }
  }
}

TEST_CASE("Different hierarchies used with fold", "[graph]")
{
  framework_graph g{levels_to_process};

  g.fold("run_add", add, concurrency::unlimited, "run", 0u)
    .input_family("number")
    .output_products("run_sum");
  g.fold("job_add", add, concurrency::unlimited).input_family("number").output_products("job_sum");

  g.observe("verify_run_sum", [](unsigned int actual) -> void { CHECK(actual == 10u); })
    .input_family("run_sum");
  g.observe("verify_job_sum",
            [](unsigned int actual) -> void {
              CHECK(actual == 20u + 45u); // 20u from events, 45u from trigger primitives
            })
    .input_family("job_sum");

  g.execute();

  CHECK(g.execution_counts("run_add") == index_limit * number_limit);
  CHECK(g.execution_counts("job_add") == index_limit * number_limit + primitive_limit);
  CHECK(g.execution_counts("verify_run_sum") == index_limit);
  CHECK(g.execution_counts("verify_job_sum") == 1);
}

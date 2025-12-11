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
//     ├ event
//     │
//     └ run
//        │
//        └ event
//
// As the run_add node performs folds only over "runs", any top-level "events"
// stores are excluded from the fold result.
//
// N.B. The multiplexer sends data products to nodes based on the name of the lowest
//      layer.  For example, the top-level "event" and the nested "run/event" are both
//      candidates for the "job" fold.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <ranges>
#include <string>
#include <vector>

using namespace phlex::experimental;

namespace {
  // Provider function
  unsigned int provide_number(data_cell_index const& index) { return index.number(); }

  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  // job -> run -> event layers
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  // job -> event levels
  constexpr auto top_level_event_limit = 10u;

  void cells_to_process(framework_driver& driver)
  {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);

    // job -> run -> event layers
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_index = job_index->make_child(i, "run");
      driver.yield(run_index);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        auto event_index = run_index->make_child(j, "event");
        driver.yield(event_index);
      }
    }

    // job -> event layers
    for (unsigned i : std::views::iota(0u, top_level_event_limit)) {
      auto tp_event_index = job_index->make_child(i, "event");
      driver.yield(tp_event_index);
    }
  }
}

TEST_CASE("Different hierarchies used with fold", "[graph]")
{
  framework_graph g{cells_to_process};

  // Register provider
  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("number"_in("event"));

  g.fold("run_add", add, concurrency::unlimited, "run", 0u)
    .input_family("number"_in("event"))
    .output_products("run_sum");
  g.fold("job_add", add, concurrency::unlimited)
    .input_family("number"_in("event"))
    .output_products("job_sum");

  g.observe("verify_run_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family("run_sum"_in("run"));
  g.observe("verify_job_sum",
            [](unsigned int actual) {
              CHECK(actual == 20u + 45u); // 20u from nested events, 45u from top-level events
            })
    .input_family("job_sum"_in("job"));

  g.execute();

  CHECK(g.execution_counts("run_add") == index_limit * number_limit);
  CHECK(g.execution_counts("job_add") == index_limit * number_limit + top_level_event_limit);
  CHECK(g.execution_counts("verify_run_sum") == index_limit);
  CHECK(g.execution_counts("verify_job_sum") == 1);
}

// =======================================================================================
// This test executes the following graph
//
//        Index Router
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
// N.B. The index_router sends data products to nodes based on the name of the lowest
//      layer.  For example, the top-level "event" and the nested "run/event" are both
//      candidates for the "job" fold.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <string>

using namespace phlex;

namespace {
  // Provider function
  unsigned int provide_number(data_cell_index const& index) { return index.number(); }

  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }
}

TEST_CASE("Different hierarchies used with fold", "[graph]")
{
  // job -> run -> event layers
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  // job -> event layers
  constexpr auto top_level_event_limit = 10u;

  experimental::layer_generator gen;
  gen.add_layer("run", {"job", index_limit});
  gen.add_layer("event", {"run", number_limit});
  gen.add_layer("event", {"job", top_level_event_limit});

  experimental::framework_graph g{driver_for_test(gen)};

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

  CHECK(g.execution_count("run_add") == index_limit * number_limit);
  CHECK(g.execution_count("job_add") == index_limit * number_limit + top_level_event_limit);
  CHECK(g.execution_count("verify_run_sum") == index_limit);
  CHECK(g.execution_count("verify_job_sum") == 1);
}

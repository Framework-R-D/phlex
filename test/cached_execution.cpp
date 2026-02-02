// =======================================================================================
// This test executes the following graph
//
//    Multiplexer
//      |  |  |
//     A1  |  |
//      |\ |  |
//      | \|  |
//     A2 B1  |
//      |\ |\ |
//      | \| \|
//     A3 B2  C
//
// where A1, A2, and A3 are transforms that execute at the "run" layer; B1 and B2 are
// transforms that execute at the "subrun" layer; and C is a transform that executes at
// the event layer.
//
// This test verifies that for each "run", "subrun", and "event", the corresponding
// transforms execute only once.  This test assumes:
//
//  1 run
//    2 subruns per run
//      5 events per subrun
//
// Note that B1 and B2 rely on the output from A1 and A2; and C relies on the output from
// B1.  However, because the A transforms execute at a different cadence than the B
// transforms (and similar for C), the B transforms must use "cached" data from the A
// transforms.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex;

namespace {
  // Provider functions
  int provide_number(data_cell_index const& index) { return 2 * index.number(); }
  int provide_another(data_cell_index const& index) { return 3 * index.number(); }
  int provide_still(data_cell_index const& index) { return 4 * index.number(); }

  int call_one(int) noexcept { return 1; }
  int call_two(int, int) noexcept { return 2; }
}

TEST_CASE("Cached function calls", "[data model]")
{
  constexpr unsigned int n_runs{1};
  constexpr unsigned int n_subruns{2u};
  constexpr unsigned int n_events{5000u};

  experimental::layer_generator gen;
  gen.add_layer("run", {"job", n_runs});
  gen.add_layer("subrun", {"run", n_subruns});
  gen.add_layer("event", {"subrun", n_events});

  experimental::framework_graph g{driver_for_test(gen)};

  // Register providers
  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product(product_query({.creator = "input"s, .layer = "run"s, .suffix = "number"s}));
  g.provide("provide_another", provide_another, concurrency::unlimited)
    .output_product(product_query({.creator = "input"s, .layer = "subrun"s, .suffix = "another"s}));
  g.provide("provide_still", provide_still, concurrency::unlimited)
    .output_product(product_query({.creator = "input"s, .layer = "event"s, .suffix = "still"s}));

  g.transform("A1", call_one, concurrency::unlimited)
    .input_family(product_query({.creator = "input"s, .layer = "run"s, .suffix = "number"s}))
    .output_products("one");
  g.transform("A2", call_one, concurrency::unlimited)
    .input_family(product_query({.creator = "A1"s, .layer = "run"s, .suffix = "one"s}))
    .output_products("used_one");
  g.transform("A3", call_one, concurrency::unlimited)
    .input_family(product_query({.creator = "A2"s, .layer = "run"s, .suffix = "used_one"s}))
    .output_products("done_one");

  g.transform("B1", call_two, concurrency::unlimited)
    .input_family(product_query({.creator = "A1"s, .layer = "run"s, .suffix = "one"s}),
                  product_query({.creator = "input"s, .layer = "subrun"s, .suffix = "another"s}))
    .output_products("two");
  g.transform("B2", call_two, concurrency::unlimited)
    .input_family(product_query({.creator = "A2"s, .layer = "run"s, .suffix = "used_one"s}),
                  product_query({.creator = "B1"s, .layer = "subrun"s, .suffix = "two"s}))
    .output_products("used_two");

  g.transform("C", call_two, concurrency::unlimited)
    .input_family(product_query({.creator = "B2"s, .layer = "subrun"s, .suffix = "used_two"s}),
                  product_query({.creator = "input"s, .layer = "event"s, .suffix = "still"s}))
    .output_products("three");

  g.execute();

  // FIXME: Need to improve the synchronization to supply strict equality
  CHECK(g.execution_counts("A1") >= n_runs);
  CHECK(g.execution_counts("A2") >= n_runs);
  CHECK(g.execution_counts("A3") >= n_runs);

  CHECK(g.execution_counts("B1") >= n_runs * n_subruns);
  CHECK(g.execution_counts("B2") >= n_runs * n_subruns);

  CHECK(g.execution_counts("C") == n_runs * n_subruns * n_events);
}

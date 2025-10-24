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
// where A1, A2, and A3 are transforms that execute at the "run" level; B1 and B2 are
// transforms that execute at the "subrun" level; and C is a transform that executes at
// the event level.
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
#include "phlex/core/specified_label.hpp"
#include "phlex/source.hpp"
#include "test/cached_execution_source.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;
using namespace test;

namespace {
  auto call_one(int) noexcept -> int { return 1; }
  auto call_two(int, int) noexcept -> int { return 2; }
}

TEST_CASE("Cached function calls", "[data model]")
{
  framework_graph g{detail::create_next<cached_execution_source>()};

  g.with("A1", call_one, concurrency::unlimited).transform("number"_in("run")).to("one");
  g.with("A2", call_one, concurrency::unlimited).transform("one"_in("run")).to("used_one");
  g.with("A3", call_one, concurrency::unlimited).transform("used_one"_in("run")).to("done_one");

  g.with("B1", call_two, concurrency::unlimited)
    .transform("one"_in("run"), "another"_in("subrun"))
    .to("two");
  g.with("B2", call_two, concurrency::unlimited)
    .transform("used_one"_in("run"), "two"_in("subrun"))
    .to("used_two");

  g.with("C", call_two, concurrency::unlimited)
    .transform("used_two"_in("subrun"), "still"_in("event"))
    .to("three");

  g.execute("cached_execution_t");

  // FIXME: Need to improve the synchronization to supply strict equality
  CHECK(g.execution_counts("A1") >= n_runs);
  CHECK(g.execution_counts("A2") >= n_runs);
  CHECK(g.execution_counts("A3") >= n_runs);

  CHECK(g.execution_counts("B1") >= n_runs * n_subruns);
  CHECK(g.execution_counts("B2") >= n_runs * n_subruns);

  CHECK(g.execution_counts("C") == n_runs * n_subruns * n_events);
}

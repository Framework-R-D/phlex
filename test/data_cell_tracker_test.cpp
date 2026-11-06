#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_cell_tracker.hpp"
#include "phlex/model/layer_path.hpp"
#include "test/ostream_logger.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <initializer_list>
#include <memory>
#include <sstream>

using namespace phlex;
using namespace phlex::detail;
using phlex::experimental::layer_path;

TEST_CASE("Test data-cell tracker", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/spill/subspill"}};

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto spill5 = run4->make_child("spill", 5);
  auto spill6 = run4->make_child("spill", 6);
  auto subspill2 = spill6->make_child("subspill", 2);
  auto run5 = job_index->make_child("run", 5);

  // Present data-cell indices in an order that reflects what a normal framework program would do.
  CHECK(tracker.report_and_evict_ready_flushes(job_index).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run4).empty());
  CHECK(tracker.report_and_evict_ready_flushes(spill5).empty());
  auto flushes = tracker.report_and_evict_ready_flushes(spill6);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == spill5);
  CHECK(flushes[0].counts->count(spill5->make_child("subspill", 0)->layer_hash()) == 0);
  CHECK(tracker.report_and_evict_ready_flushes(subspill2).empty());

  flushes = tracker.report_and_evict_ready_flushes(run5);
  REQUIRE(flushes.size() == 2);

  auto spill6_flush = flushes[0];
  CHECK(spill6_flush.index == spill6);
  REQUIRE(spill6_flush.counts->size() == 1); // Should only be "subspill" layer
  CHECK(spill6_flush.counts->count(subspill2->layer_hash()) == 1); // subspill 2

  auto run4_flush = flushes[1];
  CHECK(run4_flush.index == run4);
  REQUIRE(run4_flush.counts->size() == 1);                    // Should only be "spill" layer
  CHECK(run4_flush.counts->count(spill5->layer_hash()) == 2); // spills 5 and 6

  flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2); // job and the empty second run

  CHECK(flushes[0].index == run5);
  CHECK(flushes[0].counts->count(run5->make_child("spill", 0)->layer_hash()) == 0);
  auto job_flush = flushes[1];
  CHECK(job_flush.index == job_index);
  REQUIRE(job_flush.counts->size() == 1);                  // Should only be "run" layer
  CHECK(job_flush.counts->count(run4->layer_hash()) == 2); // runs 4 and 5
}

TEST_CASE("Data-cell tracker reports zero-count driver children", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run"}, layer_path{"run/event"}, layer_path{"event"}};

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const foreign_event = job->make_child("event", 0);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run).empty());

  auto flushes = tracker.report_and_evict_ready_flushes(foreign_event);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == run);
  REQUIRE(flushes[0].counts->size() == 1);
  CHECK(flushes[0].counts->count(run->make_child("event", 0)->layer_hash()) == 0);

  flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == job);
  CHECK(flushes[0].counts->count(run->layer_hash()) == 1);
  CHECK(flushes[0].counts->count(foreign_event->layer_hash()) == 1);
}

TEST_CASE("Data-cell tracker closes consecutive empty driver families", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/spill/event"}, layer_path{"/job/run/spill/event"}};
  // Include duplicate complete/incomplete declarations and implicit intermediate layers.
  auto const job = data_cell_index::job();
  auto const run0 = job->make_child("run", 0);
  auto const spill = run0->make_child("spill", 0);
  auto const run1 = job->make_child("run", 1);
  auto const run2 = job->make_child("run", 2);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run0).empty());
  CHECK(tracker.report_and_evict_ready_flushes(spill).empty());

  auto flushes = tracker.report_and_evict_ready_flushes(run1);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == spill);
  REQUIRE(flushes[0].counts->size() == 1);
  CHECK(flushes[0].counts->count(spill->make_child("event", 0)->layer_hash()) == 0);
  CHECK(flushes[1].index == run0);
  CHECK(flushes[1].counts->count(spill->layer_hash()) == 1);

  flushes = tracker.report_and_evict_ready_flushes(run2);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == run1);
  CHECK(flushes[0].counts->count(spill->layer_hash()) == 0);

  flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  for (auto const& flush : flushes) {
    if (flush.index == job) {
      CHECK(flush.counts->count(run0->layer_hash()) == 3);
    } else {
      CHECK(flush.index == run2);
      REQUIRE(flush.counts->size() == 1);
      CHECK(flush.counts->count(spill->layer_hash()) == 0);
    }
  }
  CHECK(tracker.report_and_evict_ready_flushes(nullptr).empty());
}

TEST_CASE("Data-cell tracker closes an empty job only for driver-owned children", "[graph]")
{
  auto const job = data_cell_index::job();
  SECTION("Declared driver children")
  {
    data_cell_tracker tracker{layer_path{"run"}};
    CHECK(tracker.report_and_evict_ready_flushes(nullptr).empty());
    CHECK(tracker.report_and_evict_ready_flushes(job).empty());
    auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
    REQUIRE(flushes.size() == 1);
    CHECK(flushes[0].index == job);
    REQUIRE(flushes[0].counts->size() == 1);
    CHECK(flushes[0].counts->count(job->make_child("run", 0)->layer_hash()) == 0);
  }
  SECTION("Children owned only by unfolds")
  {
    data_cell_tracker tracker{layer_path{"/job"}};
    CHECK(tracker.report_and_evict_ready_flushes(job).empty());
    CHECK(tracker.report_and_evict_ready_flushes(nullptr).empty());
  }
}

TEST_CASE("Data-cell tracker keeps populated completion counts sparse", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run"}, layer_path{"event"}};
  auto const job = data_cell_index::job();
  auto const event = job->make_child("event", 0);
  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(event).empty());

  auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == job);
  REQUIRE(flushes[0].counts->size() == 1);
  CHECK(flushes[0].counts->count(event->layer_hash()) == 1);
  CHECK(flushes[0].counts->count(job->make_child("run", 0)->layer_hash()) == 0);
}

TEST_CASE("Data-cell tracker tracks leaf siblings without retaining leaf indices", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/event"}};
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const event0 = run->make_child("event", 0);
  auto const event1 = run->make_child("event", 1);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run).empty());
  CHECK(tracker.report_and_evict_ready_flushes(event0).empty());
  CHECK(tracker.report_and_evict_ready_flushes(event1).empty());

  auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == run);
  CHECK(flushes[0].counts->count(event0->layer_hash()) == 2);
  CHECK(flushes[1].index == job);
  CHECK(flushes[1].counts->count(run->layer_hash()) == 1);
}

TEST_CASE("Data-cell tracker distinguishes repeated trailing layer names", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/event"}, layer_path{"calib/event"}};
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const run_event = run->make_child("event", 0);
  auto const calib = job->make_child("calib", 0);
  auto const calib_event = calib->make_child("event", 0);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run_event).empty());
  auto flushes = tracker.report_and_evict_ready_flushes(calib);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == run);
  CHECK(flushes[0].counts->count(run_event->layer_hash()) == 1);
  CHECK(tracker.report_and_evict_ready_flushes(calib_event).empty());

  flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == calib);
  CHECK(flushes[0].counts->count(calib_event->layer_hash()) == 1);
  CHECK(flushes[1].index == job);
  CHECK(flushes[1].counts->count(run->layer_hash()) == 1);
  CHECK(flushes[1].counts->count(calib->layer_hash()) == 1);
}

TEST_CASE("Data-cell tracker closes multiple active levels deepest first", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/spill/event"}, layer_path{"run/calib/event"}};
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const spill = run->make_child("spill", 0);
  auto const event = spill->make_child("event", 0);
  auto const next_run = job->make_child("run", 1);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run).empty());
  CHECK(tracker.report_and_evict_ready_flushes(spill).empty());
  CHECK(tracker.report_and_evict_ready_flushes(event).empty());
  auto const flushes = tracker.report_and_evict_ready_flushes(next_run);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == spill);
  CHECK(flushes[0].counts->count(event->layer_hash()) == 1);
  CHECK(flushes[1].index == run);
  CHECK(flushes[1].counts->count(spill->layer_hash()) == 1);
  CHECK(flushes[1].counts->count(run->make_child("calib", 0)->layer_hash()) == 0);
}

TEST_CASE("Data-cell tracker counts a family spanning multiple child layers", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/event"}, layer_path{"calib/event"}};

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto calib1 = job_index->make_child("calib", 1);
  auto run5 = job_index->make_child("run", 5);

  CHECK(tracker.report_and_evict_ready_flushes(job_index).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run4).empty());
  auto flushes = tracker.report_and_evict_ready_flushes(calib1);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == run4);
  CHECK(flushes[0].counts->count(run4->make_child("event", 0)->layer_hash()) == 0);
  flushes = tracker.report_and_evict_ready_flushes(run5);
  REQUIRE(flushes.size() == 1);
  CHECK(flushes[0].index == calib1);
  CHECK(flushes[0].counts->count(calib1->make_child("event", 0)->layer_hash()) == 0);

  flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == run5);
  CHECK(flushes[0].counts->count(run5->make_child("event", 0)->layer_hash()) == 0);
  auto job_flush = flushes[1];
  CHECK(job_flush.index == job_index);
  REQUIRE(job_flush.counts->size() == 2);                    // Should have "run" and "calib" layers
  CHECK(job_flush.counts->count(run4->layer_hash()) == 2);   // run 4 and 5
  CHECK(job_flush.counts->count(calib1->layer_hash()) == 1); // calib 1
}

TEST_CASE("Data-cell tracker rejects root indices while driver families are active", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/event"}};
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const event = run->make_child("event", 0);

  CHECK(tracker.report_and_evict_ready_flushes(job).empty());
  CHECK_THROWS_WITH(tracker.report_and_evict_ready_flushes(job),
                    "Received root index [] while driver families are active");
  CHECK(tracker.report_and_evict_ready_flushes(run).empty());
  CHECK(tracker.report_and_evict_ready_flushes(event).empty());
  CHECK_THROWS_WITH(tracker.report_and_evict_ready_flushes(job),
                    "Received root index [] while driver families are active");

  auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == run);
  REQUIRE(flushes[0].counts->size() == 1);
  CHECK(flushes[0].counts->count(event->layer_hash()) == 1);
  CHECK(flushes[1].index == job);
  REQUIRE(flushes[1].counts->size() == 1);
  CHECK(flushes[1].counts->count(run->layer_hash()) == 1);
  CHECK(tracker.report_and_evict_ready_flushes(nullptr).empty());
}

TEST_CASE("Test data-cell tracker with missing intermediate layers", "[graph]")
{
  data_cell_tracker tracker{layer_path{"run/spill"}};

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto spill2 = run4->make_child("spill", 2);

  CHECK(tracker.report_and_evict_ready_flushes(job_index).empty());

  CHECK_THROWS_WITH(tracker.report_and_evict_ready_flushes(spill2),
                    "Received index [run:4, spill:2], whose parent [run:4] does not identify an "
                    "active driver family");
  CHECK(tracker.report_and_evict_ready_flushes(run4).empty());
  auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 2);
  CHECK(flushes[0].index == run4);
  CHECK(flushes[1].index == job_index);
}

TEST_CASE("Active families at destruction generate warning message", "[graph]")
{
  std::ostringstream oss;
  auto logger = phlex::test::use_ostream_logger(oss);
  auto tracker =
    std::make_unique<data_cell_tracker>(std::initializer_list<layer_path>{layer_path{"run/event"}});

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);

  CHECK(tracker->report_and_evict_ready_flushes(job_index).empty());
  CHECK(tracker->report_and_evict_ready_flushes(run4).empty());

  tracker.reset(); // Invoke destructor to trigger warning message
  auto const warning = oss.str();
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Active families at destruction:"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Index: []"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Index: [run:4]"));
}

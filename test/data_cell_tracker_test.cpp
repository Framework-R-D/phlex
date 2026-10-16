#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_cell_tracker.hpp"
#include "test/ostream_logger.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <sstream>

using namespace phlex;
using namespace phlex::detail;

TEST_CASE("Test data-cell tracker", "[graph]")
{
  data_cell_tracker tracker;

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
  CHECK(tracker.report_and_evict_ready_flushes(spill6).empty());
  CHECK(tracker.report_and_evict_ready_flushes(subspill2).empty());

  auto flushes = tracker.report_and_evict_ready_flushes(run5);
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
  REQUIRE(flushes.size() == 1); // only job should have a flush count

  auto job_flush = flushes[0];
  CHECK(job_flush.index == job_index);
  REQUIRE(job_flush.counts->size() == 1);                  // Should only be "run" layer
  CHECK(job_flush.counts->count(run4->layer_hash()) == 2); // runs 4 and 5
}

TEST_CASE("Data-cell tracker reports zero-count driver children", "[graph]")
{
  data_cell_tracker tracker;
  tracker.set_driver_layer_paths({experimental::layer_path{"run"},
                                  experimental::layer_path{"run/event"},
                                  experimental::layer_path{"event"}});

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

TEST_CASE("Data-cell tracker closes consecutive empty driver partitions", "[graph]")
{
  data_cell_tracker tracker;
  // Include duplicate complete/incomplete declarations and implicit intermediate layers.
  tracker.set_driver_layer_paths({experimental::layer_path{"run/spill/event"},
                                  experimental::layer_path{"/job/run/spill/event"}});
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
  data_cell_tracker tracker;
  auto const job = data_cell_index::job();
  SECTION("Declared driver children")
  {
    tracker.set_driver_layer_paths({experimental::layer_path{"run"}});
    CHECK(tracker.report_and_evict_ready_flushes(job).empty());
    auto const flushes = tracker.report_and_evict_ready_flushes(nullptr);
    REQUIRE(flushes.size() == 1);
    CHECK(flushes[0].index == job);
    REQUIRE(flushes[0].counts->size() == 1);
    CHECK(flushes[0].counts->count(job->make_child("run", 0)->layer_hash()) == 0);
  }
  SECTION("Children owned only by unfolds")
  {
    tracker.set_driver_layer_paths({experimental::layer_path{"/job"}});
    CHECK(tracker.report_and_evict_ready_flushes(job).empty());
    CHECK(tracker.report_and_evict_ready_flushes(nullptr).empty());
  }
}

TEST_CASE("Data-cell tracker keeps populated completion counts sparse", "[graph]")
{
  data_cell_tracker tracker;
  tracker.set_driver_layer_paths(
    {experimental::layer_path{"run"}, experimental::layer_path{"event"}});
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

TEST_CASE("Test data-cell tracker with multiple hierarchy branches", "[graph]")
{
  data_cell_tracker tracker;

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto calib1 = job_index->make_child("calib", 1);
  auto run5 = job_index->make_child("run", 5);

  CHECK(tracker.report_and_evict_ready_flushes(job_index).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run4).empty());
  CHECK(tracker.report_and_evict_ready_flushes(calib1).empty());
  CHECK(tracker.report_and_evict_ready_flushes(run5).empty());

  auto flushes = tracker.report_and_evict_ready_flushes(nullptr);
  REQUIRE(flushes.size() == 1); // only job should have a flush count
  auto job_flush = flushes[0];
  CHECK(job_flush.index == job_index);
  REQUIRE(job_flush.counts->size() == 2);                    // Should have "run" and "calib" layers
  CHECK(job_flush.counts->count(run4->layer_hash()) == 2);   // run 4 and 5
  CHECK(job_flush.counts->count(calib1->layer_hash()) == 1); // calib 1
}

TEST_CASE("Test data-cell tracker with missing intermediate layers", "[graph]")
{
  data_cell_tracker tracker;

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto spill2 = run4->make_child("spill", 2);

  CHECK(tracker.report_and_evict_ready_flushes(job_index).empty());

  CHECK_THROWS_WITH(tracker.report_and_evict_ready_flushes(spill2),
                    "Received index [run:4, spill:2], which is not an immediate child of []");
}

TEST_CASE("Cached flush counts at destruction generate warning message", "[graph]")
{
  std::ostringstream oss;
  auto logger = phlex::test::use_ostream_logger(oss);
  auto tracker = std::make_unique<data_cell_tracker>();

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);

  CHECK(tracker->report_and_evict_ready_flushes(job_index).empty());
  CHECK(tracker->report_and_evict_ready_flushes(run4).empty());

  tracker.reset(); // Invoke destructor to trigger warning message
  auto const warning = oss.str();
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached pending flushes at destruction:"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Index: []"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("7457871974376244100 = 1"));
}

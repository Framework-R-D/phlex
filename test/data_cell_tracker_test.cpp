#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_cell_tracker.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

using namespace phlex;
using namespace phlex::experimental;

namespace {
  void use_ostream_logger(std::ostringstream& oss)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);
    auto ostream_logger = std::make_shared<spdlog::logger>("my_logger", ostream_sink);
    spdlog::set_default_logger(ostream_logger);
  }
}

TEST_CASE("Test data-cell tracker", "[graph]")
{
  data_cell_tracker tracker;

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto spill5 = run4->make_child("spill", 5);
  auto spill6 = run4->make_child("spill", 6);
  auto subspill2 = spill6->make_child("subspill", 2);
  auto run5 = job_index->make_child("run", 5);

  CHECK(tracker.closeout(job_index).empty());
  CHECK(tracker.closeout(run4).empty());
  CHECK(tracker.closeout(spill5).empty());
  CHECK(tracker.closeout(spill6).empty());
  CHECK(tracker.closeout(subspill2).empty());

  auto flushes = tracker.closeout(run5);
  REQUIRE(flushes.size() == 2);

  auto spill6_flush = flushes[0];
  CHECK(spill6_flush.index == spill6);
  REQUIRE(spill6_flush.counts->size() == 1); // Should only be "subspill" layer
  CHECK(spill6_flush.counts->count(subspill2->layer_hash()) == 1); // subspill 2

  auto run4_flush = flushes[1];
  CHECK(run4_flush.index == run4);
  REQUIRE(run4_flush.counts->size() == 1);                    // Should only be "spill" layer
  CHECK(run4_flush.counts->count(spill5->layer_hash()) == 2); // spills 5 and 6

  flushes = tracker.closeout(nullptr);
  REQUIRE(flushes.size() == 1); // only job should have a flush count

  auto job_flush = flushes[0];
  CHECK(job_flush.index == job_index);
  REQUIRE(job_flush.counts->size() == 1);                  // Should only be "run" layer
  CHECK(job_flush.counts->count(run4->layer_hash()) == 2); // runs 4 and 5
}

TEST_CASE("Test data-cell tracker with multiple hierarchy branches", "[graph]")
{
  data_cell_tracker tracker;

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);
  auto calib1 = job_index->make_child("calib", 1);
  auto run5 = job_index->make_child("run", 5);

  CHECK(tracker.closeout(job_index).empty());
  CHECK(tracker.closeout(run4).empty());
  CHECK(tracker.closeout(calib1).empty());
  CHECK(tracker.closeout(run5).empty());

  auto flushes = tracker.closeout(nullptr);
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

  CHECK(tracker.closeout(job_index).empty());

  CHECK_THROWS_WITH(tracker.closeout(spill2),
                    "Received index [run:4, spill:2], which is not an immediate child of []");
}

TEST_CASE("Cached flush counts at destruction generate warning message", "[graph]")
{
  std::ostringstream oss;
  use_ostream_logger(oss);
  auto tracker = std::make_unique<data_cell_tracker>();

  auto job_index = data_cell_index::job();
  auto run4 = job_index->make_child("run", 4);

  CHECK(tracker->closeout(job_index).empty());
  CHECK(tracker->closeout(run4).empty());

  tracker.reset(); // Invoke destructor to trigger warning message
  auto const warning = oss.str();
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Cached pending flushes at destruction:"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("Index: []"));
  CHECK_THAT(warning, Catch::Matchers::ContainsSubstring("7457871974376244100 = 1"));
}

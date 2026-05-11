// =======================================================================================
// Unit tests for child_tracker covering:
//   - single- and two-layer index hierarchies (job → runs → spills)
//   - grandchild count propagation via update_committed_counts
//   - blocking on expected_flush_count > 1 (multiple unfolds into the same parent layer)
//   - all_children_accounted() returning false before any flush message arrives
//   - concurrent execution via tbb::parallel_for with concurrent_hash_map/concurrent_vector
//
// The local flush_if_done() helper mirrors the core propagation logic from index_router,
// allowing child_tracker to be tested without a TBB flow graph.
// =======================================================================================

#include "phlex/model/child_tracker.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_cell_tracker.hpp"
#include "phlex/model/identifier.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_vector.h"
#include "oneapi/tbb/parallel_for.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

#include <memory>
#include <ranges>

using namespace phlex;
using namespace phlex::experimental;
using namespace phlex::experimental::literals;

namespace {

  void use_ostream_logger(std::ostringstream& oss)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);
    auto ostream_logger = std::make_shared<spdlog::logger>("my_logger", ostream_sink);
    spdlog::set_default_logger(ostream_logger);
  }

  // trackers_t maps index->hash() -> child_tracker_ptr.
  // 'flushed' accumulates all trackers whose send_flush() was invoked.
  using trackers_t = tbb::concurrent_hash_map<std::size_t, child_tracker_ptr>;
  using flushed_t = tbb::concurrent_vector<child_tracker_ptr>;

  child_tracker_ptr make_tracker(data_cell_index_ptr index, std::size_t expected_flush_count)
  {
    auto tracker = std::make_shared<child_tracker>(std::move(index), expected_flush_count);
    tracker->set_flush_callback([](child_tracker const&) {});
    return tracker;
  }

  void flush_if_done(data_cell_index_ptr index, trackers_t& trackers, flushed_t& flushed)
  {
    while (index) {
      trackers_t::accessor a;
      if (not trackers.find(a, index->hash())) {
        return;
      }
      auto& tracker = a->second;
      if (not tracker->all_children_accounted()) {
        return;
      }

      flushed.push_back(tracker);
      tracker->send_flush();

      auto parent = index->parent();
      if (parent) {
        if (trackers_t::accessor pa; trackers.find(pa, parent->hash())) {
          pa->second->update_committed_counts(tracker->committed_counts());
          pa->second->increment(index->layer_hash());
        }
      }
      trackers.erase(a);
      index = parent;
    }
  }
}

TEST_CASE("child_tracker: no registered callback", "[child_tracker]")
{
  std::ostringstream oss;
  use_ostream_logger(oss);
  auto tracker = std::make_shared<child_tracker>(data_cell_index::job(), 0);
  tracker->send_flush();
  CHECK_THAT(oss.str(), Catch::Matchers::ContainsSubstring("No flush callback set for index: []"));
}

TEST_CASE("child_tracker: single-layer hierarchy (job -> runs)", "[child_tracker]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run1 = job->make_child("run", 1);
  auto run_layer_hash = run0->layer_hash();

  // The unfold into run fires once, reporting 2 children.
  auto job_tracker = make_tracker(job, 0);
  job_tracker->update_expected_count(run_layer_hash, 2);

  trackers_t trackers;
  trackers.emplace(job->hash(), job_tracker);

  flushed_t flushed;

  // Run 0 completes — job tracker not yet done (run 1 still outstanding).
  job_tracker->increment(run_layer_hash);
  flush_if_done(job, trackers, flushed);
  CHECK(flushed.empty());

  // Run 1 completes — job tracker is now done.
  job_tracker->increment(run_layer_hash);
  flush_if_done(job, trackers, flushed);

  REQUIRE(flushed.size() == 1);
  auto const& jt = flushed[0];
  CHECK(jt->index() == job);
  CHECK(jt->expected_total_count() == 2);
  CHECK(jt->processed_total_count() == 2);
  CHECK(jt->committed_count_for_layer(run_layer_hash) == 2);
  CHECK(trackers.empty());
}

TEST_CASE("child_tracker: two-layer hierarchy (job -> runs -> spills)", "[child_tracker]")
{
  constexpr std::size_t n_runs = 3;
  constexpr std::size_t n_spills = 2;

  auto job = data_cell_index::job();
  auto run0 =
    job->make_child("run", 0); // representative child; layer_hash is the same for all runs
  auto run_layer_hash = run0->layer_hash();
  auto spill0 = run0->make_child("spill", 0);
  auto spill_layer_hash = spill0->layer_hash();

  trackers_t trackers;
  flushed_t flushed;

  // Create the job-layer tracker.
  auto job_tracker = make_tracker(job, 0);
  job_tracker->update_expected_count(run_layer_hash, n_runs);
  trackers.emplace(job->hash(), job_tracker);

  // Pre-populate all run trackers before running in parallel, so that flush_if_done
  // can always find the job tracker when a run completes.
  std::vector<data_cell_index_ptr> runs;
  runs.reserve(n_runs);
  for (std::size_t const r : std::views::iota(0uz, n_runs)) {
    auto& run_index = runs.emplace_back(job->make_child("run", r));
    auto run_tracker = make_tracker(run_index, 0);
    run_tracker->update_expected_count(spill_layer_hash, n_spills);
    trackers.emplace(run_index->hash(), run_tracker);
  }

  // Process each run's spills in parallel.
  tbb::parallel_for(0uz, n_runs, [&](std::size_t r) {
    trackers_t::const_accessor a;
    trackers.find(a, runs[r]->hash());
    auto const& run_tracker = a->second;
    a.release();
    for (std::size_t s = 0uz; s < n_spills; ++s) {
      run_tracker->increment(spill_layer_hash);
      flush_if_done(runs[r], trackers, flushed);
    }
  });

  REQUIRE(flushed.size() == n_runs + 1); // one per run + the job

  // Insertion order into flushed is non-deterministic under parallel execution,
  // so partition by layer name rather than relying on position.
  child_tracker_ptr job_flushed;
  for (auto const& ft : flushed) {
    if (ft->index()->layer_name() == "run"_id) {
      CHECK(ft->expected_total_count() == n_spills);
      CHECK(ft->processed_total_count() == n_spills);
      CHECK(ft->committed_count_for_layer(spill_layer_hash) == n_spills);
    } else {
      REQUIRE(ft->index() == job);
      job_flushed = ft;
    }
  }

  REQUIRE(job_flushed);
  CHECK(job_flushed->expected_total_count() == n_runs);
  CHECK(job_flushed->processed_total_count() == n_runs);
  // Immediate children (runs) counted directly.
  CHECK(job_flushed->committed_count_for_layer(run_layer_hash) == n_runs);
  // Grandchildren (spills) propagated up from the run trackers.
  CHECK(job_flushed->committed_count_for_layer(spill_layer_hash) == n_runs * n_spills);

  CHECK(trackers.empty());
}

TEST_CASE("child_tracker: multiple unfolds into the same parent layer", "[child_tracker]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run_layer_hash = run0->layer_hash();

  // Two separate unfolds each produce 2 runs — expected_flush_count = 2.
  auto job_tracker = make_tracker(job, 2);

  // Both expected-count messages arrive, each reporting 2 children.
  job_tracker->update_expected_count(run_layer_hash, 2);
  CHECK_FALSE(job_tracker->all_children_accounted()); // second flush not yet received

  job_tracker->update_expected_count(run_layer_hash, 2);
  // expected total is now 4, but no children have been processed yet.
  CHECK_FALSE(job_tracker->all_children_accounted());

  // Process all 4 children one by one; only the last call should commit.
  for (int child = 0; child < 3; ++child) {
    job_tracker->increment(run_layer_hash);
    CHECK_FALSE(job_tracker->all_children_accounted());
  }
  job_tracker->increment(run_layer_hash);
  CHECK(job_tracker->all_children_accounted());

  CHECK(job_tracker->expected_total_count() == 4);
  CHECK(job_tracker->processed_total_count() == 4);
  CHECK(job_tracker->committed_count_for_layer(run_layer_hash) == 4);
}

TEST_CASE("child_tracker: not done before any flush message arrives", "[child_tracker]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run_layer_hash = run0->layer_hash();

  auto tracker = make_tracker(job, 0);

  // No update_expected_count call yet — must return false.
  CHECK_FALSE(tracker->all_children_accounted());

  tracker->update_expected_count(run_layer_hash, 1);
  CHECK_FALSE(tracker->all_children_accounted());
  CHECK(tracker->expected_total_count() == 1);
  CHECK(tracker->processed_total_count() == 0);

  tracker->increment(run_layer_hash);
  CHECK(tracker->all_children_accounted());
}

TEST_CASE("child_tracker: update_committed_counts accumulates across multiple children",
          "[child_tracker]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run1 = job->make_child("run", 1);
  auto spill0 = run0->make_child("spill", 0);
  auto spill_layer_hash = spill0->layer_hash();
  auto run_layer_hash = run0->layer_hash();

  auto job_tracker = make_tracker(job, 0);
  job_tracker->update_expected_count(run_layer_hash, 2);

  // Simulate run 0 committing with 3 spills.
  data_cell_counts run0_committed;
  run0_committed.add_to(spill_layer_hash, 3);
  job_tracker->update_committed_counts(run0_committed);
  job_tracker->increment(run_layer_hash);

  // Simulate run 1 committing with 5 spills.
  data_cell_counts run1_committed;
  run1_committed.add_to(spill_layer_hash, 5);
  job_tracker->update_committed_counts(run1_committed);
  job_tracker->increment(run_layer_hash);

  REQUIRE(job_tracker->all_children_accounted());
  CHECK(job_tracker->committed_count_for_layer(spill_layer_hash) == 8);
  CHECK(job_tracker->committed_count_for_layer(run_layer_hash) == 2);
}

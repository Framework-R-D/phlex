// =======================================================================================
// Unit tests for flush_gate covering:
//   - single- and two-layer index hierarchies (job → runs → spills)
//   - grandchild count propagation via roll_up_child
//   - blocking on expected_flush_count > 1 (multiple unfolds into the same parent layer)
//   - all_children_accounted() returning false before any flush message arrives
//   - concurrent execution via tbb::parallel_for with concurrent_hash_map/concurrent_vector
//
// The local flush_if_done() helper mirrors the core propagation logic from index_router,
// allowing flush_gate to be tested without a TBB flow graph.  In the router, the
// expectation bookkeeping (expect_child_rollups) is performed inside apply_expected_count()
// for non-lowest child layers only; lowest-layer children are fully accounted for by the
// expected-count message itself.  These tests perform that bookkeeping directly so that
// flush_gate's readiness logic can be exercised in isolation.
// =======================================================================================

#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_gate.hpp"
#include "phlex/model/identifier.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_vector.h"
#include "oneapi/tbb/parallel_for.h"

#include <memory>
#include <ranges>
#include <vector>

using namespace phlex;
using namespace phlex::experimental;
using namespace phlex::experimental::literals;

namespace {

  // gates_t maps index->hash() -> flush_gate_ptr.
  // 'flushed' accumulates all gates whose send_flush() was invoked.
  using gates_t = tbb::concurrent_hash_map<std::size_t, flush_gate_ptr>;
  using flushed_t = tbb::concurrent_vector<flush_gate_ptr>;

  flush_gate_ptr make_gate(data_cell_index_ptr index, std::size_t expected_flush_count)
  {
    auto gate = std::make_shared<flush_gate>(std::move(index), expected_flush_count);
    gate->set_flush_callback([](flush_gate const&) {});
    return gate;
  }

  // Mirrors index_router::flush_if_done: walk up from `index`, flushing gates whose
  // children are all accounted for and rolling each gate's committed_counts into its
  // parent via roll_up_child().
  void flush_if_done(data_cell_index_ptr index, gates_t& gates, flushed_t& flushed)
  {
    while (index) {
      gates_t::accessor a;
      if (not gates.find(a, index->hash())) {
        return;
      }
      auto& gate = a->second;
      if (not gate->all_children_accounted()) {
        return;
      }

      flushed.push_back(gate);
      gate->send_flush();

      auto parent = index->parent();
      if (parent) {
        if (gates_t::accessor pa; gates.find(pa, parent->hash())) {
          pa->second->roll_up_child(gate->committed_counts());
        }
      }
      gates.erase(a);
      index = parent;
    }
  }
}

TEST_CASE("flush_gate: single-layer hierarchy (job -> runs)", "[flush_gate]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run_layer_hash = run0->layer_hash();

  // The unfold into run fires once, reporting 2 children.  Runs are lowest in this test
  // (no descendants), so the expected-count message alone is sufficient to mark the
  // job gate ready — no per-child accounting is performed.
  auto job_gate = make_gate(job, 0);
  job_gate->update_expected_count(run_layer_hash, 2);

  gates_t gates;
  gates.emplace(job->hash(), job_gate);

  flushed_t flushed;
  flush_if_done(job, gates, flushed);

  REQUIRE(flushed.size() == 1);
  auto const& jt = flushed[0];
  CHECK(jt->index() == job);
  CHECK(jt->expected_total_count() == 2);
  CHECK(jt->committed_count_for_layer(run_layer_hash) == 2);
  CHECK(gates.empty());
}

TEST_CASE("flush_gate: two-layer hierarchy (job -> runs -> spills)", "[flush_gate]")
{
  constexpr std::size_t n_runs = 3;
  constexpr std::size_t n_spills = 2;

  auto job = data_cell_index::job();
  auto run0 =
    job->make_child("run", 0); // representative child; layer_hash is the same for all runs
  auto run_layer_hash = run0->layer_hash();
  auto spill0 = run0->make_child("spill", 0);
  auto spill_layer_hash = spill0->layer_hash();

  gates_t gates;
  flushed_t flushed;

  // Create the job-layer gate.  Runs are non-lowest (they have their own gates),
  // so the job gate awaits n_runs rollups.
  auto job_gate = make_gate(job, 0);
  job_gate->expect_child_rollups(n_runs);
  job_gate->update_expected_count(run_layer_hash, n_runs);
  gates.emplace(job->hash(), job_gate);

  // Pre-populate all run gates before running in parallel, so that flush_if_done
  // can always find the job gate when a run completes.  Spills are lowest under each
  // run, so the run gates commit as soon as their expected-count message arrives.
  std::vector<data_cell_index_ptr> runs;
  runs.reserve(n_runs);
  for (std::size_t const r : std::views::iota(0uz, n_runs)) {
    auto& run_index = runs.emplace_back(job->make_child("run", r));
    auto run_gate = make_gate(run_index, 0);
    run_gate->update_expected_count(spill_layer_hash, n_spills);
    gates.emplace(run_index->hash(), run_gate);
  }

  // Flush each run in parallel.  Each run's flush_if_done will roll its committed_counts_
  // up into the job gate and, on the last one, flush the job as well.
  tbb::parallel_for(0uz, n_runs, [&](std::size_t r) { flush_if_done(runs[r], gates, flushed); });

  REQUIRE(flushed.size() == n_runs + 1); // one per run + the job

  // Insertion order into flushed is non-deterministic under parallel execution,
  // so partition by layer name rather than relying on position.
  flush_gate_ptr job_flushed;
  for (auto const& ft : flushed) {
    if (ft->index()->layer_name() == "run"_id) {
      CHECK(ft->expected_total_count() == n_spills);
      CHECK(ft->committed_count_for_layer(spill_layer_hash) == n_spills);
    } else {
      REQUIRE(ft->index() == job);
      job_flushed = ft;
    }
  }

  REQUIRE(job_flushed);
  CHECK(job_flushed->expected_total_count() == n_runs);
  // Immediate children (runs) counted directly.
  CHECK(job_flushed->committed_count_for_layer(run_layer_hash) == n_runs);
  // Grandchildren (spills) propagated up from the run gates.
  CHECK(job_flushed->committed_count_for_layer(spill_layer_hash) == n_runs * n_spills);

  CHECK(gates.empty());
}

TEST_CASE("flush_gate: multiple unfolds into the same parent layer", "[flush_gate]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run_layer_hash = run0->layer_hash();

  // Two separate unfolds each produce 2 runs — expected_flush_count = 2.  Runs are
  // lowest in this test, so once both expected-count messages have arrived the gate
  // is ready (no per-child accounting).  Until then, the higher expected_flush_count
  // gates readiness.
  auto job_gate = make_gate(job, 2);

  // First flush message: 2 children expected, but the second flush hasn't arrived yet.
  job_gate->update_expected_count(run_layer_hash, 2);
  CHECK_FALSE(job_gate->all_children_accounted());

  // Second flush message arrives — both unfolds have now reported.
  job_gate->update_expected_count(run_layer_hash, 2);
  CHECK(job_gate->all_children_accounted());

  CHECK(job_gate->expected_total_count() == 4);
  CHECK(job_gate->committed_count_for_layer(run_layer_hash) == 4);
}

TEST_CASE("flush_gate: not done before any flush message arrives", "[flush_gate]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run_layer_hash = run0->layer_hash();

  auto gate = make_gate(job, 0);

  // No update_expected_count call yet — must return false.
  CHECK_FALSE(gate->all_children_accounted());

  // After the expected-count message arrives for a lowest child layer, the gate
  // is immediately ready (no per-child accounting needed).
  gate->update_expected_count(run_layer_hash, 1);
  CHECK(gate->all_children_accounted());
  CHECK(gate->expected_total_count() == 1);
}

TEST_CASE("flush_gate: roll_up_child accumulates across multiple children", "[flush_gate]")
{
  auto job = data_cell_index::job();
  auto run0 = job->make_child("run", 0);
  auto run1 = job->make_child("run", 1);
  auto spill0 = run0->make_child("spill", 0);
  auto spill_layer_hash = spill0->layer_hash();
  auto run_layer_hash = run0->layer_hash();

  // Runs are non-lowest in this scenario (they have spills as descendants).
  auto job_gate = make_gate(job, 0);
  job_gate->expect_child_rollups(2);
  job_gate->update_expected_count(run_layer_hash, 2);

  // Simulate run 0 rolling up with 3 spills.
  data_cell_counts run0_committed;
  run0_committed.add_to(spill_layer_hash, 3);
  job_gate->roll_up_child(run0_committed);

  // Simulate run 1 rolling up with 5 spills.
  data_cell_counts run1_committed;
  run1_committed.add_to(spill_layer_hash, 5);
  job_gate->roll_up_child(run1_committed);

  REQUIRE(job_gate->all_children_accounted());
  CHECK(job_gate->committed_count_for_layer(spill_layer_hash) == 8);
  CHECK(job_gate->committed_count_for_layer(run_layer_hash) == 2);
}

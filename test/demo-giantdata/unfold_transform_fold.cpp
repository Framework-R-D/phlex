#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"
#include "plugins/layer_generator.hpp"

#include "test/demo-giantdata/user_algorithms.hpp"
#include "test/demo-giantdata/waveform_generator.hpp"
#include "test/demo-giantdata/waveform_generator_input.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>

using namespace phlex;

namespace {
  // Tracks pipeline execution to verify that fold operations begin before all unfold
  // operations complete (pipelined execution rather than batched execution).
  struct ExecutionTracker {
    // Total number of unfold operations completed
    std::atomic<std::size_t> unfold_completed{0};

    // Total number of fold operations started
    std::atomic<std::size_t> fold_started{0};

    // Maximum number of unfold operations completed when first fold started
    std::atomic<std::size_t> unfold_completed_at_first_fold{0};

    // Expected total number of operations
    std::size_t total_expected{0};
  };
}

TEST_CASE("Unfold-transform-fold pipeline", "[concurrency][unfold][fold]")
{
  // Test parameters - moderate scale to ensure sustained concurrent execution
  constexpr std::size_t n_runs = 1;
  constexpr std::size_t n_subruns = 1;
  constexpr std::size_t n_spills = 20;
  constexpr int apas_per_spill = 20;
  constexpr std::size_t wires_per_spill = apas_per_spill * 256ull;
  constexpr std::size_t chunksize = 256;

  ExecutionTracker tracker;
  tracker.total_expected = n_spills * apas_per_spill;

  // Create data layers using layer generator
  experimental::layer_generator gen;
  gen.add_layer("run", {"job", n_runs});
  gen.add_layer("subrun", {"run", n_subruns});
  gen.add_layer("spill", {"subrun", n_spills});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_wgen",
            [](data_cell_index const& spill_index) {
              return demo::WGI(wires_per_spill,
                               spill_index.parent()->parent()->number(),
                               spill_index.parent()->number(),
                               spill_index.number());
            })
    .output_product(product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "wgen"_id});

  g.unfold<demo::WaveformGenerator>(
     "WaveformGenerator",
     &demo::WaveformGenerator::predicate,
     [&tracker](demo::WaveformGenerator const& wg, std::size_t running_value) {
       auto result = wg.op(running_value, chunksize);
       tracker.unfold_completed.fetch_add(1, std::memory_order_relaxed);
       return result;
     },
     concurrency::unlimited,
     "APA")
    .input_family(product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "wgen"_id})
    .output_products("waves_in_apa");

  // Add the transform node to the graph
  auto wrapped_user_function = [](handle<demo::Waveforms> hwf) {
    return demo::clampWaveforms(*hwf);
  };

  g.transform("clamp_node", wrapped_user_function, concurrency::unlimited)
    .input_family(product_query{
      .creator = "WaveformGenerator"_id, .layer = "APA"_id, .suffix = "waves_in_apa"_id})
    .output_products("clamped_waves");

  // Add the fold node with instrumentation to detect pipelined execution
  g.fold(
     "accum_for_spill",
     [&tracker](demo::SummedClampedWaveforms& scw, handle<demo::Waveforms> hwf) {
       // Record how many unfolds had completed when the first fold started
       std::size_t expected = 0;
       tracker.unfold_completed_at_first_fold.compare_exchange_strong(
         expected, tracker.unfold_completed.load(std::memory_order_relaxed));

       tracker.fold_started.fetch_add(1, std::memory_order_relaxed);
       demo::accumulateSCW(scw, *hwf);
     },
     concurrency::unlimited,
     "spill")
    .input_family(
      product_query{.creator = "clamp_node"_id, .layer = "APA"_id, .suffix = "clamped_waves"_id})
    .output_products("summed_waveforms");

  // Execute the graph
  g.execute();

  // Verify pipelined execution: first fold started before all unfolds completed
  auto const unfolds_at_first_fold = tracker.unfold_completed_at_first_fold.load();
  CHECK(unfolds_at_first_fold < tracker.total_expected);

  // Verify all operations completed
  CHECK(tracker.unfold_completed == tracker.total_expected);
  CHECK(tracker.fold_started == tracker.total_expected);
}

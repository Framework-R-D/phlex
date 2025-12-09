#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/async_driver.hpp"
#include "test/products_for_output.hpp"

#include "test/demo-giantdata/user_algorithms.hpp"
#include "test/demo-giantdata/waveform_generator.hpp"
#include "test/demo-giantdata/waveform_generator_input.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

using namespace phlex::experimental;

// Call the program as follows:
// ./unfold_transform_fold [number of spills [APAs per spill]]
int main(int argc, char* argv[])
{

  std::vector<std::string> const args(argv + 1, argv + argc);
  std::size_t const n_runs = [&args]() {
    if (args.size() > 1) {
      return std::stoul(args[0]);
    }
    return 1ul;
  }();

  std::size_t const n_subruns = [&args]() {
    if (args.size() > 2) {
      return std::stoul(args[1]);
    }
    return 1ul;
  }();

  std::size_t const n_spills = [&args]() {
    if (args.size() > 2) {
      return std::stoul(args[1]);
    }
    return 1ul;
  }();

  int const apas_per_spill = [&args]() {
    if (args.size() > 3) {
      return std::stoi(args[2]);
    }
    return 150;
  }();

  std::size_t const wires_per_spill = apas_per_spill * 256ull;

  // Create some data layers of the data-layer hierarchy.
  // We may or may not want to create pre-generated data layers.
  // Each data layer gets an index number in the hierarchy.

  auto source = [n_runs, n_subruns, n_spills](framework_driver& driver) {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);

    // job -> run -> subrun -> spill data layers
    for (unsigned runno : std::views::iota(0u, n_runs)) {
      auto run_index = job_index->make_child(runno, "run");
      driver.yield(run_index);

      for (unsigned subrunno : std::views::iota(0u, n_subruns)) {
        auto subrun_index = run_index->make_child(subrunno, "subrun");
        driver.yield(subrun_index);

        for (unsigned spillno : std::views::iota(0u, n_spills)) {

          auto spill_index = subrun_index->make_child(spillno, "spill");
          driver.yield(spill_index);
        }
      }
    }
  };

  // Create the graph. The source tells us what data we will process.
  // We introduce a new scope to make sure the graph is destroyed before we
  // write out the logged records.
  {
    framework_graph g{source};

    // Add the unfold node to the graph. We do not yet know how to provide the chunksize
    // to the constructor of the WaveformGenerator, so we will use the default value.
    auto const chunksize = 256LL; // this could be read from a configuration file

    g.provide("provide_wgen",
              [wires_per_spill](data_cell_index const& spill_index) {
                return demo::WGI(wires_per_spill,
                                 spill_index.parent()->parent()->number(), // ugh
                                 spill_index.parent()->number(),
                                 spill_index.number());
              })
      .output_product("wget"_in("spill"));

    g.unfold<demo::WaveformGenerator>(
       "WaveformGenerator",
       &demo::WaveformGenerator::predicate,
       [](demo::WaveformGenerator const& wg, std::size_t running_value) {
         return wg.op(running_value, chunksize);
       },
       concurrency::unlimited,
       "APA")
      .input_family("wgen"_in("spill"))
      .output_products("waves_in_apa");

    // Add the transform node to the graph.
    auto wrapped_user_function = [](phlex::experimental::handle<demo::Waveforms> hwf) {
      return demo::clampWaveforms(*hwf);
    };

    g.transform("clamp_node", wrapped_user_function, concurrency::unlimited)
      .input_family("waves_in_apa"_in("APA"))
      .output_products("clamped_waves");

    // Add the fold node to the graph.
    g.fold(
       "accum_for_spill",
       [](demo::SummedClampedWaveforms& scw, phlex::experimental::handle<demo::Waveforms> hwf) {
         demo::accumulateSCW(scw, *hwf);
       },
       concurrency::unlimited,
       "spill" // partition the output by the spill
       )
      .input_family("clamped_waves"_in("APA"))
      .output_products("summed_waveforms");

    g.make<test::products_for_output>().output(
      "save", &test::products_for_output::save, concurrency::serial);

    // Execute the graph.
    g.execute();
  }
}

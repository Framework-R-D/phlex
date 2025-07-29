#include "phlex/core/framework_graph.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/async_driver.hpp"
#include "test/products_for_output.hpp"

#include "test/demo-giantdata/log_record.hpp"
#include "test/demo-giantdata/user_algorithms.hpp"
#include "test/demo-giantdata/waveform_generator.hpp"
#include "test/demo-giantdata/waveform_generator_input.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <string>
#include <vector>

using framework_driver = phlex::experimental::async_driver<phlex::experimental::product_store_ptr>;
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

  // Create some levels of the data set categories hierarchy.
  // We may or may not want to create pre-generated data set categories like this.
  // Each data set category gets an index number in the hierarchy.

  auto source = [n_runs, n_subruns, n_spills, wires_per_spill](framework_driver& driver) {
    auto job_store = product_store::base();
    driver.yield(job_store);

    // job -> run -> subrun -> spill levels
    for (unsigned runno : std::views::iota(0u, n_runs)) {
      auto run_store = job_store->make_child(runno, "run");
      driver.yield(run_store);

      for (unsigned subrunno : std::views::iota(0u, n_subruns)) {
        auto subrun_store = run_store->make_child(subrunno, "subrun");
        driver.yield(subrun_store);

        for (unsigned spillno : std::views::iota(0u, n_spills)) {

          auto spill_store = subrun_store->make_child(spillno, "spill");

          // Put the WGI product into the job, so that our CHOF can find it.
          auto next_size = wires_per_spill;
          demo::log_record("add_wgi",
                           run_store->id()->number(),
                           subrun_store->id()->number(),
                           spill_store->id()->number(),
                           -1,
                           &spill_store,
                           next_size,
                           nullptr);
          // NOTE: the only reason that we are able to put the spill id into the WGI object
          // is because we have access to the store
          spill_store->add_product<demo::WGI>("wgen",
                                              demo::WGI(next_size,
                                                        run_store->id()->number(),
                                                        subrun_store->id()->number(),
                                                        spill_store->id()->number()));

          driver.yield(spill_store);
        }
      }
    }
  };

  // Create the graph. The source tells us what data we will process.
  // We introduce a new scope to make sure the graph is destroyed before we
  // write out the logged records.
  demo::log_record("create_graph");
  {
    framework_graph g{source};

    // g.with<X> is "registration code" as implemented in phlex.
    // Each call to g.with<> creates a new CHOF in the graph.

    // Add the unfold node to the graph. We do not yet know how to provide the chunksize
    // to the constructor of the WaveformGenerator, so we will use the default value.
    demo::log_record("add_unfold");
    auto const chunksize = 256LL; // this could be read from a configuration file

    g.with<demo::WaveformGenerator>(
       &demo::WaveformGenerator::predicate,
       [](demo::WaveformGenerator const& wg, std::size_t running_value) {
         return wg.op(running_value, chunksize);
       },
       concurrency::unlimited)
      .unfold("wgen"_in("spill")) // the type of node to create
      .into("waves_in_apa")       // label the chunks we create as "waves_in_apa"
      .within_family("APA")       // put the chunks into a data set category called "APA"
      ;

    // Add the transform node to the graph.
    demo::log_record("add_transform");
    auto wrapped_user_function = [](phlex::experimental::handle<demo::Waveforms> hwf) {
      auto apa_id = hwf.level_id().number();
      auto spill_id = hwf.level_id().parent()->number();
      auto subrun_id = hwf.level_id().parent()->parent()->number();
      auto run_id = hwf.level_id().parent()->parent()->parent()->number();
      return demo::clampWaveforms(*hwf, run_id, subrun_id, spill_id, apa_id);
    };

    // Because we are using a closure here, we have to provide a node name;
    // the framework is unable to deduce the name of the "function".
    g.with("clamp_node", wrapped_user_function, concurrency::unlimited)
      .transform("waves_in_apa") // the type of node to create, and the label of the input
      .for_each("APA")
      .to("clamped_waves") // label the chunks we create as "clamped_waves"
      ;

    // Add the fold node to the graph.
    demo::log_record("add_fold");
    g.with(
       "accum_for_spill",
       [](demo::SummedClampedWaveforms& scw, phlex::experimental::handle<demo::Waveforms> hwf) {
         auto apa_id = hwf.level_id().number();
         auto spill_id = hwf.level_id().parent()->number();
         auto subrun_id = hwf.level_id().parent()->parent()->number();
         auto run_id = hwf.level_id().parent()->parent()->parent()->number();
         demo::accumulateSCW(scw, *hwf, run_id, subrun_id, spill_id, apa_id);
       },
       concurrency::unlimited)
      .fold("clamped_waves"_in("APA"))
      .to("summed_waveforms")
      .partitioned_by("spill") // partition the output by the spill
      ;

    demo::log_record("add_output");
    g.make<test::products_for_output>().output_with(
      "save", &test::products_for_output::save, concurrency::serial);

    // Execute the graph.
    demo::log_record("execute_graph");
    g.execute("unfold_transform_fold");
    demo::log_record("end_graph");
  }
  demo::log_record("graph_destroyed");
  demo::write_log("unfold_transform_fold.tsv");
}

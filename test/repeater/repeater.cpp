#include "index_router.hpp"
#include "nodes.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"
#include "phlex/utilities/resource_usage.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/cfg/env.h"

#include <map>
#include <string>

using namespace oneapi::tbb;
using namespace phlex;

namespace {
  constexpr int n_runs = 1;
  constexpr int n_subruns = 10;
  constexpr int n_spills = 50;
}

TEST_CASE("Test repeater", "[multithreading]")
{
  experimental::resource_usage usage;

  spdlog::cfg::load_env_levels();
  spdlog::set_pattern("[thread %t] [%^%-7l%$] %v");

  flow::graph g;

  experimental::layer_generator gen;
  gen.add_layer("run", {"job", n_runs});
  gen.add_layer("subrun", {"run", n_subruns});
  gen.add_layer("spill", {"subrun", n_spills});

  framework_driver driver{driver_for_test(gen)};

  // Providers
  test::provider_node<int> run_provider{g, "run", "geometry", 1};
  test::provider_node<double> subrun_provider{g, "subrun", "calib", 2.5};
  test::provider_node<char> spill_provider{g, "spill", "hits", 'a'};

  // Single-argument consumers
  test::consumer_node<int> run_consumer{g, "consume_geometry", "run"};
  test::consumer_node<double> subrun_consumer{g, "consume_calib", "subrun"};
  test::consumer_node<char> spill_consumer{g, "consume_hits", "spill"};

  // Multi-argument consumers
  test::multiarg_consumer_node<int, double> run_subrun_consumer{
    g, "consume_geometry_calib", {"run", "subrun"}};
  test::multiarg_consumer_node<int, char> run_spill_consumer{
    g, "consume_geometry_hits", {"run", "spill"}};
  test::multiarg_consumer_node<double, char> subrun_spill_consumer{
    g, "consume_calib_hits", {"subrun", "spill"}};
  test::multiarg_consumer_node<int, double, char> all_consumer{
    g, "consume_all", {"run", "subrun", "spill"}};

  // Get all multi-layer combinations
  // FIXME: An optimization is to make sure these combinations are unique
  std::map<std::string, test::named_index_ports> multilayers;
  multilayers.try_emplace(run_subrun_consumer.name(), run_subrun_consumer.index_ports());
  multilayers.try_emplace(run_spill_consumer.name(), run_spill_consumer.index_ports());
  multilayers.try_emplace(subrun_spill_consumer.name(), subrun_spill_consumer.index_ports());
  multilayers.try_emplace(all_consumer.name(), all_consumer.index_ports());

  test::index_router router{g, gen.layer_paths(), std::move(multilayers)};

  flow::input_node src{
    g, [done = false, &driver, &router](flow_control& fc) mutable -> flow::continue_msg {
      if (done) {
        fc.stop();
        return {};
      }

      auto item = driver();
      if (not item) {
        done = true;
        router.shutdown();
        return {};
      }
      router.route(*item);
      return {};
    }};

  // The input_node must be connected to *something* to ensure that the graph runs.
  flow::function_node<flow::continue_msg> dummy{
    g, flow::unlimited, [](flow::continue_msg) -> flow::continue_msg { return {}; }};
  make_edge(src, dummy);

  // Connect index-set nodes to providers
  make_edge(router.index_node_for("run"), run_provider);
  make_edge(router.index_node_for("subrun"), subrun_provider);
  make_edge(router.index_node_for("spill"), spill_provider);

  // Connect providers to one-argument consumers
  make_edge(run_provider, run_consumer);
  make_edge(subrun_provider, subrun_consumer);
  make_edge(spill_provider, spill_consumer);

  // Connect providers to multi-argument consumers
  make_edge(run_provider, input_port<0>(run_subrun_consumer));
  make_edge(subrun_provider, input_port<1>(run_subrun_consumer));

  make_edge(run_provider, input_port<0>(run_spill_consumer));
  make_edge(spill_provider, input_port<1>(run_spill_consumer));

  make_edge(subrun_provider, input_port<0>(subrun_spill_consumer));
  make_edge(spill_provider, input_port<1>(subrun_spill_consumer));

  make_edge(run_provider, input_port<0>(all_consumer));
  make_edge(subrun_provider, input_port<1>(all_consumer));
  make_edge(spill_provider, input_port<2>(all_consumer));

  src.activate();
  g.wait_for_all();

  // Single-argument function-call checks
  CHECK(run_provider.calls() == n_runs);
  CHECK(subrun_provider.calls() == n_runs * n_subruns);
  CHECK(spill_provider.calls() == n_runs * n_subruns * n_spills);

  CHECK(run_consumer.calls() == n_runs);
  CHECK(subrun_consumer.calls() == n_runs * n_subruns);
  CHECK(spill_consumer.calls() == n_runs * n_subruns * n_spills);

  // Multi-argument function-call checks
  CHECK(run_subrun_consumer.calls() == n_runs * n_subruns);
  CHECK(run_spill_consumer.calls() == n_runs * n_subruns * n_spills);
  CHECK(subrun_spill_consumer.calls() == n_runs * n_subruns * n_spills);
  CHECK(all_consumer.calls() == n_runs * n_subruns * n_spills);
}

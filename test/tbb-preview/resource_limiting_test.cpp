#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <string>
#include <string_view>
#include <tuple>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace {

  // ROOT is the representation of the ROOT resource.
  struct ROOT {};

  // GENIE is the representation of the GENIE resource.
  struct GENIE {};

  // DB is the representation of the DB resource.
  // The value id is the database ID.
  struct DB {
    unsigned int id;
  };

  void start(std::string_view algorithm, unsigned int spill, unsigned int data = 0u)
  {
    spdlog::info("Start\t{}\t{}\t{}", algorithm, spill, data);
  }

  void stop(std::string_view algorithm, unsigned int spill, unsigned int data = 0u)
  {
    spdlog::info("Stop\t{}\t{}\t{}", algorithm, spill, data);
  }

  void setup_file_logger(std::string const& test_name)
  {
    auto logger = spdlog::basic_logger_mt(test_name, test_name + ".txt", true);
    spdlog::set_default_logger(logger);
  }

} // namespace

TEST_CASE("Serialize functions based on resource", "[multithreading]")
{
  setup_file_logger("serialize_functions_based_on_resource");
  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) {
                         if (i < 10) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  std::atomic<unsigned int> root_counter{};
  std::atomic<unsigned int> genie_counter{};

  ROOT const root_resource{};

  flow::resource_provider<ROOT const*> root_resource_provider{&root_resource};
  flow::resource_provider<GENIE> genie_resource{GENIE{}};

  flow::resource_limited_node<unsigned int, std::tuple<unsigned int>> node1{
    g,
    flow::unlimited,
    std::tie(root_resource_provider),
    [&root_counter](unsigned int const i, auto& outputs, ROOT const*) {
      thread_counter c{root_counter};
      spdlog::info("Processing from node 1 {} with root token", i);
      std::get<0>(outputs).try_put(i);
    }};

  flow::resource_limited_node<unsigned int, std::tuple<unsigned int>> node2{
    g,
    flow::unlimited,
    std::tie(root_resource_provider, genie_resource),
    [&root_counter, &genie_counter](unsigned int const i, auto& outputs, ROOT const*, GENIE) {
      thread_counter c1{root_counter};
      thread_counter c2{genie_counter};
      spdlog::info("Processing from node 2 {}", i);
      std::get<0>(outputs).try_put(i);
    }};

  flow::resource_limited_node<unsigned int, std::tuple<unsigned int>> node3{
    g,
    flow::unlimited,
    std::tie(genie_resource),
    [&genie_counter](unsigned int const i, auto& outputs, GENIE) {
      thread_counter c{genie_counter};
      spdlog::info("Processing from node 3 {}", i);
      std::get<0>(outputs).try_put(i);
    }};

  auto receiving_node_for = [](flow::graph& g, std::string const& label) {
    return flow::function_node<unsigned int, unsigned int>{
      g, flow::unlimited, [label](unsigned int const i) {
        spdlog::info("Processed {} task {}", label, i);
        return i;
      }};
  };

  auto receiving_node_1 = receiving_node_for(g, "ROOT");
  auto receiving_node_2 = receiving_node_for(g, "ROOT/GENIE");
  auto receiving_node_3 = receiving_node_for(g, "GENIE");

  make_edge(src, node1);
  make_edge(src, node2);
  make_edge(src, node3);

  make_edge(node1, receiving_node_1);
  make_edge(node2, receiving_node_2);
  make_edge(node3, receiving_node_3);

  src.activate();
  g.wait_for_all();
}

TEST_CASE("Serialize functions in diamond graph", "[multithreading]")
{
  setup_file_logger("serialize_functions_in_diamond_graph");
  flow::graph g;
  flow::input_node src{g, [i = 0u](flow_control& fc) mutable -> unsigned int {
                         if (i < 10u) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  flow::resource_provider<ROOT> root_resource{ROOT{}};

  std::atomic<unsigned int> root_counter{};

  auto serial_node_for = [&root_resource, &root_counter](auto& g, int label) {
    return flow::resource_limited_node<unsigned int, std::tuple<unsigned int>>{
      g,
      flow::unlimited,
      std::tie(root_resource),
      [&root_counter, label](unsigned int const i, auto& outputs, ROOT) {
        thread_counter c{root_counter};
        spdlog::info("Processing from node {} {}", label, i);
        std::get<0>(outputs).try_put(i);
      }};
  };

  auto node1 = serial_node_for(g, 1);
  auto node2 = serial_node_for(g, 2);
  auto node3 = serial_node_for(g, 3);

  make_edge(src, node1);
  make_edge(src, node2);
  make_edge(node1, node3);
  make_edge(node2, node3);

  src.activate();
  g.wait_for_all();
}

TEST_CASE("Test based on oneTBB PR 1677 (RFC)", "[multithreading]")
{
  using namespace std::chrono_literals;

  setup_file_logger("test_based_on_onetbb_pr_1677");

  // We first print a message that describes the different fields in our log messages
  spdlog::set_pattern("%v");
  spdlog::info("time\tthread\tevent\tnode\tmessage\tdata");

  // Now set the actual pattern for remaining logged messages
  spdlog::set_pattern("%H:%M:%S.%f\t%t\t%v");
  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) {
                         if (i < 50) {
                           start("Source", i + 1); // The message is the spill id we will emit
                           auto j = ++i;
                           stop("Source", i);
                           return j;
                         }
                         fc.stop();
                         return 0u;
                       }};

  // Declare the counters that we use to verify that the resource constraints are being met.
  std::atomic<unsigned int> root_counter{};
  std::atomic<unsigned int> genie_counter{};
  std::atomic<unsigned int> db_counter{};

  flow::resource_provider<ROOT> root_limiter{ROOT{}};
  flow::resource_provider<GENIE> genie_limiter{GENIE{}};

  DB const db1{1};
  DB const db13{13};
  flow::resource_provider<DB const*> db_limiter{&db1, &db13};

  auto fill_histo = [&root_counter](unsigned int const i, auto& outputs, ROOT) {
    thread_counter c{root_counter};
    start("Histogramming", i);
    spin_for(10ms);
    stop("Histogramming", i);
    std::get<0>(outputs).try_put(i);
  };

  auto gen_fill_histo = [&root_counter,
                         &genie_counter](unsigned int const i, auto& outputs, ROOT, GENIE) {
    thread_counter c1{root_counter};
    thread_counter c2{genie_counter};
    start("Histo-generating", i);
    spin_for(10ms);
    stop("Histo-generating", i);
    std::get<0>(outputs).try_put(i);
  };

  auto generate = [&genie_counter](unsigned int const i, auto& outputs, GENIE) {
    thread_counter c{genie_counter};
    start("Generating", i);
    spin_for(10ms);
    stop("Generating", i);
    std::get<0>(outputs).try_put(i);
  };

  auto propagate = [](unsigned int const i) -> unsigned int {
    start("Propagating", i);
    spin_for(150ms);
    stop("Propagating", i);
    return i;
  };

  using rl_node = flow::resource_limited_node<unsigned int, std::tuple<unsigned int>>;

  rl_node histogrammer{g, flow::unlimited, std::tie(root_limiter), fill_histo};
  rl_node histo_generator{
    g, flow::unlimited, std::tie(root_limiter, genie_limiter), gen_fill_histo};
  rl_node generator{g, flow::unlimited, std::tie(genie_limiter), generate};
  flow::function_node<unsigned int, unsigned int> propagator{g, flow::unlimited, propagate};

  // Nodes that use the DB resource limited to 2 tokens
  auto make_calibrator = [&db_counter](std::string_view algorithm) {
    return [&db_counter, algorithm](unsigned int const i, auto& outputs, DB const* db) {
      thread_counter c{db_counter, 2};
      start(algorithm, i, db->id);
      spin_for(10ms);
      stop(algorithm, i, db->id);
      std::get<0>(outputs).try_put(i);
    };
  };

  rl_node calibratorA{g, flow::unlimited, std::tie(db_limiter), make_calibrator("Calibration[A]")};
  rl_node calibratorB{g, flow::unlimited, std::tie(db_limiter), make_calibrator("Calibration[B]")};
  rl_node calibratorC{g, flow::serial, std::tie(db_limiter), make_calibrator("Calibration[C]")};

  make_edge(src, histogrammer);
  make_edge(src, histo_generator);
  make_edge(src, generator);
  make_edge(src, propagator);
  make_edge(src, calibratorA);
  make_edge(src, calibratorB);
  make_edge(src, calibratorC);

  src.activate();
  g.wait_for_all();
}

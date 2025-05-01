#include "phlex/graph/serial_node.hpp"
#include "phlex/utilities/thread_counter.hpp"

#include "catch2/catch_all.hpp"
#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <string>

using namespace phlex::experimental;
using namespace oneapi::tbb;
using namespace spdlog;

TEST_CASE("Serialize functions based on resource", "[multithreading]")
{
  flow::graph g;
  unsigned int i{};
  flow::input_node src{g, [&i](flow_control& fc) {
                         if (i < 10) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  serializers serialized_resources{g};

  std::atomic<unsigned int> root_counter{}, genie_counter{};

  serial_node<unsigned int, 1> node1{
    g, serialized_resources.get("ROOT"), [&root_counter](unsigned int const i) {
      thread_counter c{root_counter};
      debug("Processing from node 1 {}", i);
      return i;
    }};

  serial_node<unsigned int, 2> node2{g,
                                     serialized_resources.get("ROOT", "GENIE"),
                                     [&root_counter, &genie_counter](unsigned int const i) {
                                       thread_counter c1{root_counter};
                                       thread_counter c2{genie_counter};
                                       debug("Processing from node 2 {}", i);
                                       return i;
                                     }};

  serial_node<unsigned int, 1> node3{
    g, serialized_resources.get("GENIE"), [&genie_counter](unsigned int const i) {
      thread_counter c{genie_counter};
      debug("Processing from node 3 {}", i);
      return i;
    }};

  serial_node<unsigned int, 0> node4{
    g, tbb::flow::unlimited, [](unsigned int const i) { return i; }};

  auto receiving_node_for = [](tbb::flow::graph& g, std::string const& label) {
    return flow::function_node<unsigned int, unsigned int>{
      g, flow::unlimited, [&label](unsigned int const i) {
        debug("Processed {} task {}", label, i);
        return i;
      }};
  };

  auto receiving_node_1 = receiving_node_for(g, "ROOT");
  auto receiving_node_2 = receiving_node_for(g, "ROOT/GENIE");
  auto receiving_node_3 = receiving_node_for(g, "GENIE");
  auto receiving_node_4 = receiving_node_for(g, "unlimited");

  make_edge(src, node1);
  make_edge(src, node2);
  make_edge(src, node3);
  make_edge(src, node4);

  make_edge(node1, receiving_node_1);
  make_edge(node2, receiving_node_2);
  make_edge(node3, receiving_node_3);
  make_edge(node4, receiving_node_4);

  serialized_resources.activate();
  src.activate();
  g.wait_for_all();
}

TEST_CASE("Serialize functions in unfold/merge graph", "[multithreading]")
{
  flow::graph g;
  flow::input_node src{g, [i = 0u](flow_control& fc) mutable {
                         if (i < 10u) {
                           return ++i;
                         }
                         fc.stop();
                         return 0u;
                       }};

  serializers serialized_resources{g};

  std::atomic<unsigned int> root_counter{};

  auto root_resource = serialized_resources.get("ROOT");
  auto serial_node_for = [&root_resource, &root_counter](auto& g, int label) {
    return serial_node<unsigned int, 1>{
      g, root_resource, [&root_counter, label](unsigned int const i) {
        thread_counter c{root_counter};
        debug("Processing from node {} {}", label, i);
        return i;
      }};
  };

  auto node1 = serial_node_for(g, 1);
  auto node2 = serial_node_for(g, 2);
  auto node3 = serial_node_for(g, 3);

  make_edge(src, node1);
  make_edge(src, node2);
  make_edge(node1, node3);
  make_edge(node2, node3);

  serialized_resources.activate();
  src.activate();
  g.wait_for_all();
}

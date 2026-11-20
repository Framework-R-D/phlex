#include "phlex/core/node_builder.hpp"
#include "test/resources_for_testing.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <concepts>
#include <tuple>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;

namespace {
  // Zero-resource node shapes.
  static_assert(std::same_as<node_builder<int, no_outputs_t, std::tuple<>>::node_t,
                             tbb::flow::function_node<int>>);
  static_assert(std::same_as<node_builder<int, std::tuple<int>, std::tuple<>>::node_t,
                             tbb::flow::function_node<int, int>>);
  static_assert(std::same_as<node_builder<int, multifunction_outputs<int>, std::tuple<>>::node_t,
                             tbb::flow::multifunction_node<int, std::tuple<int>>>);

  // Resource-dependent node selection.
  static_assert(
    std::same_as<node_builder<int, no_outputs_t, std::tuple<unlimited_test_resource>>::node_t,
                 tbb::flow::function_node<int>>);
  static_assert(
    std::same_as<node_builder<int, std::tuple<int>, std::tuple<unlimited_test_resource>>::node_t,
                 tbb::flow::function_node<int, int>>);
  static_assert(
    std::same_as<
      node_builder<int, multifunction_outputs<int>, std::tuple<unlimited_test_resource>>::node_t,
      tbb::flow::multifunction_node<int, std::tuple<int>>>);
  static_assert(
    std::same_as<node_builder<int, no_outputs_t, std::tuple<pointer_token_resource>>::node_t,
                 tbb::flow::resource_limited_node<int, no_outputs_t>>);
}

int main() {}

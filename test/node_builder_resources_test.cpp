#include "phlex/concurrency.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/resource/catalog.hpp"
#include "test/node_builder_helpers.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <oneapi/tbb/flow_graph.h>

#include <atomic>
#include <tuple>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;

TEST_CASE("node builder supplies unlimited resources", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_test_resource>();
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<unlimited_test_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, unlimited_test_resource const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves unlimited then serialized resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_test_resource>();
  resources.add_serialized<pointer_token_resource>();
  std::atomic<bool> called{};

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<unlimited_test_resource, pointer_token_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, unlimited_test_resource const*, pointer_token_resource const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves serialized then unlimited resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_test_resource>();
  resources.add_serialized<pointer_token_resource>();
  std::atomic<bool> called{};

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<pointer_token_resource, unlimited_test_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, pointer_token_resource const*, unlimited_test_resource const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies value resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<value_token_resource>(42);
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<value_token_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, value_token_resource token) {
      CHECK(value == 43);
      CHECK(token.value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies custom resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<custom_token_resource>();
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<custom_token_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, int token) {
      CHECK(value == 43);
      CHECK(token == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();
  CHECK(called);
}

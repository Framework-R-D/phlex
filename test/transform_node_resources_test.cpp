#include "phlex/core/declared_transform.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource/catalog.hpp"
#include "phlex/core/transform_node.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"
#include "test/node_test_products.hpp"
#include "test/product_store_helpers.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <utility>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::experimental;
using namespace phlex::experimental::literals;
using namespace phlex::test;

namespace {
  constexpr auto message_id = 42u;

  output_type_1 increment_with_resource(input_type_1 const& input, pointer_token_resource const*)
  {
    return {input.value + 1};
  }

  output_type_1 increment_with_unlimited_resource(input_type_1 const& input,
                                                  unlimited_test_resource const*)
  {
    return {input.value + 1};
  }

  template <typename T>
  product_selector selector(algorithm_name const& creator)
  {
    return {
      .creator = creator.algorithm(), .layer = "job", .suffix = "", .type = make_type_id<T>()};
  }

  template <typename F>
  auto algorithm_bits_for(F function)
  {
    return algorithm_bits{std::shared_ptr<void_tag>{}, std::move(function)};
  }
}

TEST_CASE("transform_node receives a resource token", "[transform_node][resource]")
{
  oneapi::tbb::flow::graph graph;
  auto const input_creator_name = algorithm_name::create("input");
  identifier const stage_name = "test_stage"_id;
  auto input_selector = selector<input_type_1>(input_creator_name);
  auto input_store = store_with_product(
    gsl::not_null{&input_creator_name}, gsl::not_null{&stage_name}, input_type_1{21});
  auto alg = algorithm_bits_for(increment_with_resource);
  resource_catalog resources;
  resources.add_serialized<pointer_token_resource>();

  transform_node<decltype(alg), pointer_token_resource> node{
    algorithm_name{"increment_with_resource"},
    "test_stage"_id,
    1u,
    {},
    graph,
    std::move(alg),
    {input_selector},
    {},
    resources};
  declared_transform& transform = node;

  oneapi::tbb::flow::queue_node<message> sink{graph};
  make_edge(transform.output_port(), sink);

  REQUIRE(node.port(input_selector).try_put({.store = input_store, .id = message_id}));
  graph.wait_for_all();

  message output;
  REQUIRE(sink.try_get(output));
  CHECK(output.store->get_product<output_type_1>(
          gsl::make_not_null(std::addressof(transform.output()[0]))) == output_type_1{22});
  CHECK(transform.num_calls() == 1u);
}

TEST_CASE("transform_node receives an unlimited resource", "[transform_node][resource]")
{
  oneapi::tbb::flow::graph graph;
  auto const input_creator_name = algorithm_name::create("input");
  identifier const stage_name = "test_stage"_id;
  auto input_selector = selector<input_type_1>(input_creator_name);
  auto input_store = store_with_product(
    gsl::not_null{&input_creator_name}, gsl::not_null{&stage_name}, input_type_1{21});
  auto alg = algorithm_bits_for(increment_with_unlimited_resource);
  resource_catalog resources;
  resources.add_unlimited<unlimited_test_resource>();

  transform_node<decltype(alg), unlimited_test_resource> node{
    algorithm_name{"increment_with_unlimited_resource"},
    "test_stage"_id,
    1u,
    {},
    graph,
    std::move(alg),
    {input_selector},
    {},
    resources};
  declared_transform& transform = node;

  oneapi::tbb::flow::queue_node<message> sink{graph};
  make_edge(transform.output_port(), sink);

  REQUIRE(node.port(input_selector).try_put({.store = input_store, .id = message_id}));
  graph.wait_for_all();

  message output;
  REQUIRE(sink.try_get(output));
  REQUIRE(output.store);
  CHECK(output.store->get_product<output_type_1>(
          gsl::make_not_null(std::addressof(transform.output()[0]))) == output_type_1{22});
  CHECK(transform.num_calls() == 1u);
}

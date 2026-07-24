#include "phlex/core/declared_transform.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/flow_graph.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::experimental;

namespace {
  constexpr auto message_id = 42u;

  struct input_type_1 {
    int value;
    auto operator<=>(input_type_1 const&) const = default;
  };

  struct input_type_2 {
    int value;
    auto operator<=>(input_type_2 const&) const = default;
  };

  struct output_type_1 {
    int value;
    auto operator<=>(output_type_1 const&) const = default;
  };

  struct output_type_2 {
    std::string value;
    auto operator<=>(output_type_2 const&) const = default;
  };

  output_type_1 double_value(input_type_1 const& input) { return {input.value * 2}; }

  output_type_1 add_values(input_type_1 const& left, input_type_2 const& right)
  {
    return {left.value + right.value};
  }

  auto number_and_label(input_type_1 const& input)
  {
    return std::tuple{output_type_1{input.value}, output_type_2{std::to_string(input.value)}};
  }

  template <typename T>
  product_specification spec(char const* creator, char const* suffix)
  {
    return {algorithm_name{creator}, identifier{suffix}, make_type_id<T>()};
  }

  template <typename T>
  product_selector selector(char const* creator, char const* suffix)
  {
    return {.creator = creator, .layer = "job", .suffix = suffix, .type = make_type_id<T>()};
  }

  template <typename T>
  product_store_ptr store_with_product(char const* creator, char const* suffix, T value)
  {
    auto store = product_store::base(creator);
    store->add_product(spec<T>(creator, suffix), std::move(value));
    return store;
  }

  template <typename F>
  auto algorithm_bits_for(F function)
  {
    return algorithm_bits{std::shared_ptr<void_tag>{}, std::move(function)};
  }
}

TEST_CASE("transform_node directly transforms one input product", "[transform_node]")
{
  oneapi::tbb::flow::graph graph;
  auto input_selector = selector<input_type_1>("input", "");
  auto input_store = store_with_product("input", "", input_type_1{21});
  auto alg = algorithm_bits_for(double_value);

  transform_node<decltype(alg)> node{
    algorithm_name{"double_value"}, 1u, {}, graph, std::move(alg), {input_selector}, {}};
  declared_transform& transform = node;

  auto const& output_specs = transform.output();
  REQUIRE(output_specs.size() == 1u);
  CHECK(output_specs[0].creator() == algorithm_name{"double_value"});
  CHECK(output_specs[0].suffix() == identifier{""});
  CHECK(output_specs[0].type() == make_type_id<output_type_1>());

  oneapi::tbb::flow::queue_node<message> sink{graph};
  make_edge(transform.output_port(), sink);

  REQUIRE(node.port(input_selector).try_put({.store = input_store, .id = message_id}));
  graph.wait_for_all();

  message output;
  REQUIRE(sink.try_get(output));

  CHECK(output.id == message_id);
  REQUIRE(output.store);
  CHECK(output.store->index() == input_store->index());
  CHECK(output.store->source() == algorithm_name{"double_value"});
  CHECK(output.store->get_product<output_type_1>(output_specs[0]) == output_type_1{42});

  CHECK(transform.num_calls() == 1u);
  CHECK(transform.product_count() == 1u);
}

TEST_CASE("transform_node joins multiple input products", "[transform_node]")
{
  oneapi::tbb::flow::graph graph;
  auto left_selector = selector<input_type_1>("left_input", "");
  auto right_selector = selector<input_type_2>("right_input", "");
  auto left_store = store_with_product("left_input", "", input_type_1{17});
  auto right_store = store_with_product("right_input", "", input_type_2{25});
  auto alg = algorithm_bits_for(add_values);

  transform_node<decltype(alg)> node{algorithm_name{"add_values"},
                                     1u,
                                     {},
                                     graph,
                                     std::move(alg),
                                     {left_selector, right_selector},
                                     {}};
  declared_transform& transform = node;

  auto const& output_specs = transform.output();
  REQUIRE(output_specs.size() == 1u);
  CHECK(output_specs[0].suffix() == identifier{""});

  oneapi::tbb::flow::queue_node<message> sink{graph};
  make_edge(transform.output_port(), sink);

  auto& left_port = *transform.ports().at(0);
  auto& right_port = *transform.ports().at(1);

  REQUIRE(left_port.try_put({.store = left_store, .id = message_id}));
  graph.wait_for_all();

  message output;
  CHECK_FALSE(sink.try_get(output));
  CHECK(transform.num_calls() == 0u);

  REQUIRE(right_port.try_put({.store = right_store, .id = message_id}));
  graph.wait_for_all();
  CHECK_FALSE(sink.try_get(output));
  CHECK(transform.num_calls() == 0u);

  auto index_ports = transform.index_ports();
  REQUIRE(index_ports.size() == 2u);
  REQUIRE(index_ports[0].index_port->try_put(
    {.index = left_store->index(), .msg_id = message_id, .cache = true}));
  graph.wait_for_all();
  CHECK_FALSE(sink.try_get(output));
  CHECK(transform.num_calls() == 0u);

  REQUIRE(index_ports[1].index_port->try_put(
    {.index = right_store->index(), .msg_id = message_id, .cache = true}));
  graph.wait_for_all();

  REQUIRE(sink.try_get(output));
  CHECK_FALSE(sink.try_get(output));

  REQUIRE(index_ports[0].token_port->try_put({.index = left_store->index(), .count = 1}));
  REQUIRE(index_ports[1].token_port->try_put({.index = right_store->index(), .count = 1}));
  graph.wait_for_all();

  CHECK(output.id == message_id);
  REQUIRE(output.store);
  CHECK(output.store->index() == left_store->index());

  CHECK(output.store->get_product<output_type_1>(output_specs[0]) == output_type_1{42});

  CHECK(transform.num_calls() == 1u);
  CHECK(transform.product_count() == 1u);
}

TEST_CASE("transform_node stores multiple output products", "[transform_node]")
{
  oneapi::tbb::flow::graph graph;
  auto input_selector = selector<input_type_1>("input", "");
  auto input_store = store_with_product("input", "", input_type_1{7});
  auto alg = algorithm_bits_for(number_and_label);

  transform_node<decltype(alg)> node{algorithm_name{"number_and_label"},
                                     1u,
                                     {},
                                     graph,
                                     std::move(alg),
                                     {input_selector},
                                     {"number", "label"}};
  declared_transform& transform = node;

  oneapi::tbb::flow::queue_node<message> sink{graph};
  make_edge(transform.output_port(), sink);

  REQUIRE(node.port(input_selector).try_put({.store = input_store, .id = message_id}));
  graph.wait_for_all();

  message output;
  REQUIRE(sink.try_get(output));
  CHECK_FALSE(sink.try_get(output));

  auto const& output_specs = transform.output();
  REQUIRE(output_specs.size() == 2u);
  CHECK(output_specs[0].creator() == algorithm_name{"number_and_label"});
  CHECK(output_specs[0].suffix() == identifier{"number"});
  CHECK(output_specs[0].type() == make_type_id<output_type_1>());
  CHECK(output_specs[1].creator() == algorithm_name{"number_and_label"});
  CHECK(output_specs[1].suffix() == identifier{"label"});
  CHECK(output_specs[1].type() == make_type_id<output_type_2>());

  REQUIRE(output.store);
  CHECK(output.store->get_product<output_type_1>(output_specs[0]) == output_type_1{7});
  CHECK(output.store->get_product<output_type_2>(output_specs[1]) == output_type_2{"7"});

  CHECK(transform.num_calls() == 1u);
  CHECK(transform.product_count() == 1u);
}

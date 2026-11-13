#include "phlex/core/fold_join_node.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"

#include <catch2/catch_test_macros.hpp>
#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::experimental;
using namespace phlex::experimental::literals;

namespace {
  constexpr auto message_id = 42u;

  struct input_type_1 {
    int value;
  };

  struct input_type_2 {
    int value;
  };

  template <typename T>
  product_specification spec(algorithm_name const& creator)
  {
    return {creator, ""_id, make_type_id<T>()};
  }

  template <typename T>
  product_store_ptr store_with_product(gsl::not_null<algorithm_name const*> creator,
                                       gsl::not_null<identifier const*> stage,
                                       T value)
  {
    auto store = product_store::base(creator, stage);
    store->add_product(spec<T>(*creator), std::move(value));
    return store;
  }
}

TEST_CASE("multilayer_join_node joins multiple input products", "[join]")
{
  oneapi::tbb::flow::graph graph;

  auto const stage = "test_stage"_id;
  auto const left_creator_name = algorithm_name::create("left_input");
  auto const right_creator_name = algorithm_name::create("right_input");

  auto left_store = store_with_product(
    gsl::make_not_null(&left_creator_name), gsl::make_not_null(&stage), input_type_1{17});
  auto right_store = store_with_product(
    gsl::make_not_null(&right_creator_name), gsl::make_not_null(&stage), input_type_2{25});

  // Force repeaters by passing distinct layer names.
  // The actual routing is performed by matching index hashes between data, index, and flush.
  auto join = multilayer_join_node<2>{
    graph,
    "multilayer_join_test",
    std::vector<identifier>{identifier{"left_layer"}, identifier{"right_layer"}}};

  oneapi::tbb::flow::queue_node<message_tuple<2>> sink{graph};
  make_edge(output_port<0>(join), sink);

  auto& left_port = receiver_for<0ull, 2>(join, 0u);
  auto& right_port = receiver_for<0ull, 2>(join, 1u);

  CHECK_THROWS_AS(receiver_for<0ull>(join, 2u), std::runtime_error);

  REQUIRE(left_port.try_put({.store = left_store, .id = message_id}));
  graph.wait_for_all();

  message_tuple<2> output;
  CHECK_FALSE(sink.try_get(output));

  REQUIRE(right_port.try_put({.store = right_store, .id = message_id}));
  graph.wait_for_all();
  CHECK_FALSE(sink.try_get(output));

  auto index_ports = join.index_ports();
  REQUIRE(index_ports.size() == 2u);
  CHECK(index_ports[0].layer == "left_layer"_id);
  CHECK(index_ports[1].layer == "right_layer"_id);
  CHECK_FALSE(index_ports[0].counting_layer.has_value());
  CHECK_FALSE(index_ports[1].counting_layer.has_value());
  REQUIRE(index_ports[0].index_port->try_put(
    {.index = left_store->index(), .msg_id = message_id, .cache = true}));
  graph.wait_for_all();
  CHECK_FALSE(sink.try_get(output));

  REQUIRE(index_ports[1].index_port->try_put(
    {.index = right_store->index(), .msg_id = message_id, .cache = true}));
  graph.wait_for_all();

  REQUIRE(sink.try_get(output));
  CHECK_FALSE(sink.try_get(output));

  CHECK(std::get<0>(output).id == message_id);
  CHECK(std::get<1>(output).id == message_id);
  REQUIRE(std::get<0>(output).store);
  REQUIRE(std::get<1>(output).store);
  CHECK(std::get<0>(output).store->index() == left_store->index());
  CHECK(std::get<1>(output).store->index() == right_store->index());

  // Do what is necessary to have the tokens flushed, so that the test does not generate
  // warnings.
  REQUIRE(index_ports[0].token_port->try_put({.index = left_store->index(), .count = 1}));
  REQUIRE(index_ports[1].token_port->try_put({.index = right_store->index(), .count = 1}));
  graph.wait_for_all();
}

TEST_CASE("Same-layer join receiver lookup and input ordering", "[join]")
{
  tbb::flow::graph graph;
  multilayer_join_node<3> join{graph, "same_layer_join", {"event"_id, "event"_id, "event"_id}};
  CHECK(join.index_ports().empty());

  auto const left_creator = algorithm_name::create("left");
  auto const middle_creator = algorithm_name::create("middle");
  auto const right_creator = algorithm_name::create("right");

  product_selectors const selectors{{.creator = left_creator.to_string()},
                                    {.creator = middle_creator.to_string()},
                                    {.creator = right_creator.to_string()}};
  auto const stage = "test"_id;
  std::array const stores{product_store::base(&left_creator, &stage),
                          product_store::base(&middle_creator, &stage),
                          product_store::base(&right_creator, &stage)};
  tbb::flow::queue_node<message_tuple<3>> sink{graph};

  using internal::receiver_for;
  auto try_put_for_receiver = [&join, &selectors, &stores](std::size_t const index) {
    return receiver_for<3>(join, selectors, selectors[index])
      .try_put({.store = stores.at(index), .id = message_id});
  };

  make_edge(output_port<0>(join), sink);
  SECTION("joins wait for all inputs and preserve selector order")
  {
    REQUIRE(try_put_for_receiver(2));
    REQUIRE(try_put_for_receiver(0));
    graph.wait_for_all();
    message_tuple<3> output;
    CHECK_FALSE(sink.try_get(output));
    REQUIRE(try_put_for_receiver(1));
    graph.wait_for_all();
    REQUIRE(sink.try_get(output));
    CHECK(std::get<0>(output).id == message_id);
    CHECK(std::get<1>(output).id == message_id);
    CHECK(std::get<2>(output).id == message_id);
    CHECK(std::get<0>(output).store == stores[0]);
    CHECK(std::get<1>(output).store == stores[1]);
    CHECK(std::get<2>(output).store == stores[2]);
    CHECK_FALSE(sink.try_get(output));
  }

  SECTION("receiver lookup rejects unknown selectors")
  {
    CHECK_THROWS_AS(receiver_for<3>(join, selectors, {.creator = "missing"_id}),
                    std::runtime_error);
  }
}

TEST_CASE("Same-layer joins match message IDs rather than arrival order", "[join]")
{
  tbb::flow::graph graph;
  multilayer_join_node<3> join{graph, "same_layer_id_join", {"event"_id, "event"_id, "event"_id}};
  tbb::flow::queue_node<message_tuple<3>> sink{graph};
  make_edge(output_port<0>(join), sink);

  auto try_put = [&join](std::array<std::size_t, 3> const message_ids) {
    unsigned int valid_puts = 0;
    // This test verifies ID matching, so real product stores are unnecessary.
    valid_puts += receiver_for<0ull, 3>(join, 0).try_put({.id = message_ids[0]});
    valid_puts += receiver_for<1ull, 3>(join, 1).try_put({.id = message_ids[1]});
    valid_puts += receiver_for<2ull, 3>(join, 2).try_put({.id = message_ids[2]});
    return valid_puts == 3u;
  };

  // Every port has a message, but neither ID has a complete input set.
  REQUIRE(try_put({42u, 42u, 43u}));
  graph.wait_for_all();
  message_tuple<3> output;
  CHECK_FALSE(sink.try_get(output));

  REQUIRE(try_put({43u, 43u, 42u}));
  graph.wait_for_all();

  // Only two complete sets of messages should be produced.
  std::vector<message_tuple<3>> outputs;
  CHECK(sink.try_get(output));
  outputs.push_back(output);
  CHECK(sink.try_get(output));
  outputs.push_back(output);
  CHECK_FALSE(sink.try_get(output));

  // All message IDs should agree per output
  std::set<std::size_t> consumed_message_ids;
  for (auto const& [msg0, msg1, msg2] : outputs) {
    CHECK(msg0.id == msg1.id);
    CHECK(msg1.id == msg2.id);
    consumed_message_ids.insert(msg0.id);
  }

  CHECK(consumed_message_ids == std::set<std::size_t>{42u, 43u});
}

TEST_CASE("Fold join receiver lookup and index metadata", "[join]")
{
  tbb::flow::graph graph;
  auto const name = algorithm_name::create("fold_join_test");
  fold_join_node<int, 2> join{graph,
                              name,
                              "test_stage"_id,
                              "job"_id,
                              {"event"_id, "run"_id},
                              product_specifications(1),
                              [](data_cell_index const&) { return std::make_unique<int>(0); }};
  SECTION("data receivers exclude the partition port and reject invalid indices")
  {
    CHECK_THROWS_AS(receiver_for<1ull>(join, 2u), std::runtime_error);
    CHECK_THROWS_AS(receiver_for<1ull>(join, std::numeric_limits<std::size_t>::max()),
                    std::runtime_error);
    product_selectors const selectors{{.creator = "left"_id}, {.creator = "right"_id}};
    CHECK(&receiver_for(join, selectors, selectors[0]) == &input_port<1>(join));
    CHECK(&receiver_for(join, selectors, selectors[1]) == &input_port<2>(join));
  }

  SECTION("index ports identify routing and counting layers with the deepest input first")
  {
    auto const ports = join.index_ports();
    REQUIRE(ports.size() == 3);
    CHECK(ports[0].layer == "job"_id);
    CHECK(ports[0].counting_layer == "event"_id);
    CHECK(ports[1].layer == "event"_id);
    CHECK(ports[1].counting_layer == "event"_id);
    CHECK(ports[2].layer == "run"_id);
    CHECK(ports[2].counting_layer == "run"_id);
  }
}

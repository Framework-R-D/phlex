#include "phlex/core/multilayer_join_node.hpp"
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
  };

  struct input_type_2 {
    int value;
  };

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
}

TEST_CASE("multilayer_join_node joins multiple input products", "[join]")
{
  oneapi::tbb::flow::graph graph;
  auto left_store = store_with_product("left_input", "", input_type_1{17});
  auto right_store = store_with_product("right_input", "", input_type_2{25});

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

  REQUIRE(left_port.try_put({.store = left_store, .id = message_id}));
  graph.wait_for_all();

  message_tuple<2> output;
  CHECK_FALSE(sink.try_get(output));

  REQUIRE(right_port.try_put({.store = right_store, .id = message_id}));
  graph.wait_for_all();
  CHECK_FALSE(sink.try_get(output));

  auto index_ports = join.index_ports();
  REQUIRE(index_ports.size() == 2u);
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

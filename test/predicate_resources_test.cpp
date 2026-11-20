#include "phlex/core/framework_graph.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;

namespace {
  unsigned int give_me_nums(data_cell_index const& id) { return id.number() - 1; }

  bool evens_with_resource(unsigned int const value, pointer_token_resource const*)
  {
    return value % 2u == 0u;
  }
}

TEST_CASE("Predicate receives a resource token", "[filtering][resource]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = 10, .start_at = 1});
  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_serialized_resource<pointer_token_resource>();
  g.provide("provide_num", give_me_nums, concurrency::unlimited)
    .output_product("input", "num", "event");
  g.predicate("evens_with_resource", evens_with_resource, concurrency::unlimited)
    .input_family({.creator = "input", .layer = "event", .suffix = "num"},
                  resource<pointer_token_resource>{});
  g.observe(
     "collect_evens", [](unsigned int) {}, concurrency::unlimited)
    .input_family({.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("evens_with_resource");

  g.execute();

  CHECK(g.execution_count("evens_with_resource") == 10);
  CHECK(g.execution_count("collect_evens") == 5);
}

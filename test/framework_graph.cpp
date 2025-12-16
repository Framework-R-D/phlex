#include "phlex/core/framework_graph.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <stdexcept>

using namespace phlex::experimental;

TEST_CASE("Catch STL exceptions", "[graph]")
{
  framework_graph g{[](framework_driver&) { throw std::runtime_error("STL error"); }};
  CHECK_THROWS_AS(g.execute(), std::exception);
}

TEST_CASE("Catch other exceptions", "[graph]")
{
  framework_graph g{[](framework_driver&) { throw 2.5; }};
  CHECK_THROWS_AS(g.execute(), double);
}

TEST_CASE("Make progress with one thread", "[graph]")
{
  layer_generator gen;
  gen.add_layer("spill", {"job", 1000});

  framework_graph g{driver_for_test(gen), 1};
  g.provide(
     "provide_number",
     [](data_cell_index const& index) -> unsigned int { return index.number(); },
     concurrency::unlimited)
    .output_product("number"_in("spill"));
  g.observe(
     "observe_number", [](unsigned int const /*number*/) {}, concurrency::unlimited)
    .input_family("number"_in("spill"));
  g.execute();

  CHECK(gen.emitted_cells("/job/spill") == 1000);
  CHECK(g.execution_counts("provide_number") == 1000);
  CHECK(g.execution_counts("observe_number") == 1000);
}

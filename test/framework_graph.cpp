#include "phlex/core/framework_graph.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <stdexcept>

using namespace phlex;

TEST_CASE("Catch STL exceptions", "[graph]")
{
  experimental::framework_graph g{[](framework_driver&) { throw std::runtime_error("STL error"); }};
  CHECK_THROWS_AS(g.execute(), std::exception);
}

TEST_CASE("Catch other exceptions", "[graph]")
{
  experimental::framework_graph g{[](framework_driver&) { throw 2.5; }};
  CHECK_THROWS_AS(g.execute(), double);
}

TEST_CASE("Make progress with one thread", "[graph]")
{
  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", 1000});

  experimental::framework_graph g{driver_for_test(gen), 1};
  g.provide(
     "provide_number",
     [](data_cell_index const& index) -> unsigned int { return index.number(); },
     concurrency::unlimited)
    .output_product(
      product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "number"_id});
  g.observe(
     "observe_number", [](unsigned int const /*number*/) {}, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "number"_id});
  g.execute();

  CHECK(gen.emitted_cells("/job/spill") == 1000);
  CHECK(g.execution_counts("provide_number") == 1000);
  CHECK(g.execution_counts("observe_number") == 1000);
}

TEST_CASE("Stop driver when workflow throws exception", "[graph]")
{
  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", 1000});

  experimental::framework_graph g{driver_for_test(gen)};
  g.provide(
     "throw_exception",
     [](data_cell_index const&) -> unsigned int {
       throw std::runtime_error("Error to stop driver");
     },
     concurrency::unlimited)
    .output_product("number"_in("spill"));

  // Must have at least one downstream node that requires something of the
  // provider...otherwise provider will not be executed.
  g.observe(
     "downstream_of_exception", [](unsigned int) {}, concurrency::unlimited)
    .input_family("number"_in("spill"));

  CHECK_THROWS(g.execute());

  // There are N + 1 potential existing threads for a framework job, where N corresponds
  // to the number configured by the user, and 1 corresponds to the separate std::jthread
  // created by the async_driver.  Each "pull" from the async_driver happens in a
  // serialized way.  However, once an index has been pulled from the async_driver by the
  // flow graph, that index is sent to downstream nodes for further processing.
  //
  // The first node that processes that index is a provider that immediately throws an
  // exception.  This places the framework graph in an error state, where the async_driver
  // is short-circuited from doing further processing.
  //
  // We make the assumption that one of those threads will trigger the exception and the
  // remaining threads must be permitted to complete.
  CHECK(gen.emitted_cells("/job/spill") <=
        static_cast<std::size_t>(experimental::max_allowed_parallelism::active_value() + 1));

  // A node has not "executed" until it has returned successfully.  For that reason,
  // neither the "throw_exception" provider nor the "downstream_of_exception" observer
  // will have executed.
  CHECK(g.execution_counts("throw_exception") == 0ull);
  CHECK(g.execution_counts("downstream_of_exception") == 0ull);
}

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

  CHECK(gen.emitted_cell_count("/job/spill") == 1000);
  CHECK(g.execution_count("provide_number") == 1000);
  CHECK(g.execution_count("observe_number") == 1000);
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
    .output_product(
      product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "number"_id});

  // Must have at least one downstream node that requires something of the
  // provider...otherwise provider will not be executed.
  g.observe(
     "downstream_of_exception", [](unsigned int) {}, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "number"_id});

  CHECK_THROWS(g.execute());

  // The framework will see one fewer data cells than were emitted by the generator (for
  // the data layer in which the exception was thrown).
  //
  // With the current implementation, it is possible that framework graph will not see the
  // "/job/spill" data layer before the job ends.  In that case, the "/job/spill" layer
  // will not have been recorded, and we therefore allow it to be "missing", which is what
  // the 'true' argument allows for.
  CHECK(gen.emitted_cell_count("/job/spill") == g.seen_cell_count("/job/spill", true) + 1u);

  // A node has not "executed" until it has returned successfully.  For that reason,
  // neither the "throw_exception" provider nor the "downstream_of_exception" observer
  // will have executed.
  CHECK(g.execution_count("throw_exception") == 0ull);
  CHECK(g.execution_count("downstream_of_exception") == 0ull);
}

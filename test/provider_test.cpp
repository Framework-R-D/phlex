#include "phlex/core/framework_graph.hpp"
#include "phlex/core/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

using namespace phlex;
using Catch::Matchers::ContainsSubstring;

namespace toy {
  struct VertexCollection {
    std::size_t data;
  };
  auto make_collection(std::size_t i) { return VertexCollection{i}; }
}

namespace {
  // Provider algorithms
  toy::VertexCollection give_me_vertices(data_cell_index const& id)
  {
    spdlog::info("give_me_vertices: {}", id.number());
    return toy::make_collection(id.number());
  }

  // Type-erased provider function
  experimental::product_ptr give_me_vertices_erased(data_cell_index const& id)
  {
    spdlog::info("give_me_vertices_erased: {}", id.number());
    return std::make_unique<experimental::product<toy::VertexCollection>>(
      toy::make_collection(id.number()));
  }

  // Vertices source for implicit provider test
  class vertices_source : public experimental::source {
  public:
    experimental::provider_bundles create_providers(product_selector const& selector) override
    {
      using namespace experimental;
      provider_bundles bundles;
      std::string const layer = "spill";
      std::string const stage = "previous_process";
      product_specification spec{
        "vertices_maker", "happy_vertices", make_type_id<toy::VertexCollection>()};

      if (selector.match(spec, identifier{layer}, identifier{stage})) {
        bundles.push_back(provider_bundle{.provider_function = give_me_vertices_erased,
                                          .max_concurrency = concurrency::unlimited,
                                          .spec = std::move(spec),
                                          .layer = layer,
                                          .stage = stage});
      }
      return bundles;
    }
    index_generator indices() override { co_return; }
  };

  unsigned pass_on(toy::VertexCollection const& vertices) { return vertices.data; }
}

TEST_CASE("Explicit providers")
{
  constexpr auto num_spills{3u};

  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", num_spills, 1u});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("my_name_here", give_me_vertices, concurrency::unlimited)
    .output_product("vertices_maker", "happy_vertices", "spill");

  g.transform("passer", pass_on, concurrency::unlimited)
    .input_family(
      product_selector{.creator = "vertices_maker", .layer = "spill", .suffix = "happy_vertices"});

  g.execute();

  CHECK(g.execution_count("passer") == num_spills);
  CHECK(g.execution_count("my_name_here") == num_spills);
}

TEST_CASE("Implicit providers")
{
  constexpr auto num_spills{3u};

  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", num_spills, 1u});

  experimental::framework_graph g{driver_for_test(gen)};

  g.source<vertices_source>("vertices_source");

  g.transform("passer", pass_on, concurrency::unlimited)
    .input_family(
      product_selector{.creator = "vertices_maker", .layer = "spill", .suffix = "happy_vertices"});

  g.observe(
     "verify_implicit_stage", [](toy::VertexCollection const&) {}, concurrency::unlimited)
    .input_family(
      product_selector{.creator = "vertices_maker", .layer = "spill", .suffix = "happy_vertices"});
  g.execute();

  CHECK(g.execution_count("vertices_maker") == num_spills);
  CHECK(g.execution_count("passer") == num_spills);
  CHECK(g.execution_count("verify_implicit_stage") == num_spills);
}

TEST_CASE("Throw when two sources with the same name are registered")
{
  experimental::framework_graph g;
  g.source<vertices_source>("vertices_source");
  g.source<vertices_source>("vertices_source");

  CHECK_THROWS_WITH(g.execute(),
                    ContainsSubstring("Source with name 'vertices_source' already exists"));
}

TEST_CASE("Throw when two implicit providers are found for the same product")
{
  experimental::framework_graph g;

  // Register two sources that can provide the same product
  g.source<vertices_source>("vertices_source_1");
  g.source<vertices_source>("vertices_source_2");

  g.transform("passer", pass_on, concurrency::unlimited)
    .input_family(
      product_selector{.creator = "vertices_maker", .layer = "spill", .suffix = "happy_vertices"});

  CHECK_THROWS_WITH(
    g.execute(),
    ContainsSubstring(
      "Multiple implicit providers found for product 'vertices_maker/happy_vertices") &&
      ContainsSubstring("spill") && ContainsSubstring("passer"));
}

TEST_CASE("Throw when no provider found for required product")
{
  experimental::framework_graph g;

  // Register an observer that needs a product from a creator that does not exist in the graph.
  // Since there is no matching provider, make_computational_edges should throw listing all
  // unmatched products.
  g.observe(
     "observer", [](unsigned int const) {}, concurrency::unlimited)
    .input_family(product_selector{.creator = "nonexistent_creator", .layer = "job"});

  CHECK_THROWS_WITH(g.execute(),
                    ContainsSubstring("No provider found for the following required products:") &&
                      ContainsSubstring("nonexistent_creator") && ContainsSubstring("job"));
}

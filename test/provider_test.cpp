#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

using namespace phlex;

namespace toy {
  struct VertexCollection {
    std::size_t data;
  };
}

namespace {
  // Provider algorithms
  toy::VertexCollection give_me_vertices(data_cell_index const& id)
  {
    spdlog::info("give_me_vertices: {}", id.number());
    return toy::VertexCollection{id.number()};
  }
}

namespace {
  unsigned pass_on(toy::VertexCollection const& vertices) { return vertices.data; }
}

TEST_CASE("provider_test")
{
  constexpr auto max_events{3u};
  // constexpr auto max_events{1'000'000u};
  spdlog::flush_on(spdlog::level::trace);

  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", max_events, 1u});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("my_name_here", give_me_vertices, concurrency::unlimited)
    .output_product(
      product_query{.creator = "input", .layer = "spill", .suffix = "happy_vertices"});

  g.transform("passer", pass_on, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "spill", .suffix = "happy_vertices"})
    .output_products("vertex_data");

  g.execute();
  CHECK(g.execution_count("passer") == max_events);
  CHECK(g.execution_count("my_name_here") == max_events);
}

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

using namespace phlex::experimental;

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

  auto levels_to_process = [](framework_driver& driver) {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);

    for (unsigned int i : std::views::iota(1u, max_events + 1)) {
      auto index = job_index->make_child(i, "spill");
      driver.yield(index);
    }
  };

  framework_graph g{levels_to_process};

  g.provide("my_name_here", give_me_vertices, concurrency::unlimited)
    .output_product("happy_vertices"_in("spill"));

  g.transform("passer", pass_on, concurrency::unlimited)
    .input_family("happy_vertices"_in("spill"))
    .output_products("vertex_data");

  spdlog::info("graph: is about to be executed");
  try {
    g.execute();
    CHECK(g.execution_counts("passer") == max_events);
    CHECK(g.execution_counts("my_name_here") == max_events);
  } catch (std::exception const& e) {
    spdlog::error("Exception during graph execution: {}", e.what());
    throw;
  } catch (...) {
    spdlog::error("Unknown exception during graph execution");
    throw;
  }
}

/*

Planned development flow:

[x] Get initial test working.
[x] Make the data product be a simple struct not a fundamental type.
[x] Introduce stub `provider` that takes product_store_ptr as input and returns a product_store_ptr.
    Wire the provider into the graph.
[ ] Modify the `provider` to take data_cell_index as input.
    The `multiplexer` will then need to emit data_cell_index, stripped from the `product_store` it currently returns.
[ ] Modify the `multiplexer` to accept data_cell_index as input.
    The Input will then need to emit data_cell_index, stripped from the `product_store` it currently returns.
[ ] Modify `Input` to accept data_cell_index is input.
    This will mean `framework_driver` will have to emit data_cell_index instead of product_store.

Then we're done.

*/

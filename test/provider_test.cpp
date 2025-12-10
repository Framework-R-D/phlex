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

  g.execute();
  CHECK(g.execution_counts("passer") == max_events);
  CHECK(g.execution_counts("my_name_here") == max_events);
}

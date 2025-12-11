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

TEST_CASE("provider_with_flush")
{
  // Test that providers handle flush messages correctly
  // Flush messages should pass through providers without triggering execution
  constexpr auto max_events{2u};
  spdlog::flush_on(spdlog::level::warn);

  auto levels_to_process = [](framework_driver& driver) {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);

    for (unsigned int i : std::views::iota(1u, max_events + 1)) {
      auto spill_index = job_index->make_child(i, "spill");
      driver.yield(spill_index);

      // Send a flush after each spill - this tests the flush handling path
      auto flush_store = product_store::base()->make_flush();
      driver.yield(flush_store->id());
    }
  };

  framework_graph g{levels_to_process};

  g.provide("flush_aware_provider", give_me_vertices, concurrency::unlimited)
    .output_product("flush_vertices"_in("spill"));

  g.transform("flush_consumer", pass_on, concurrency::unlimited)
    .input_family("flush_vertices"_in("spill"))
    .output_products("vertex_data");

  g.execute();
  // Flush messages should not count as provider executions
  CHECK(g.execution_counts("flush_consumer") == max_events);
  CHECK(g.execution_counts("flush_aware_provider") == max_events);
}

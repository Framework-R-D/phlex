#include "phlex/core/framework_graph.hpp"
#include "plugins/layer_generator.hpp"

using namespace phlex::experimental;

namespace {
  unsigned pass_on(unsigned number) { return number; }
}

int main()
{
  // spdlog::flush_on(spdlog::level::trace);

  constexpr auto max_events{100'000u};

  layer_generator gen;
  gen.add_layer("event", {"job", max_events, 1u});

  framework_graph g{driver_for_test(gen)};

  g.provide("provide_number", [](data_cell_index const& id) -> unsigned { return id.number(); })
    .output_product("number"_in("event"));
  g.transform("pass_on", pass_on, concurrency::unlimited)
    .input_family("number"_in("event"))
    .output_products("different");
  g.execute();
}

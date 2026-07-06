#include "phlex/core/framework_graph.hpp"
#include "plugins/layer_generator.hpp"

#include <iostream>

using namespace phlex;
namespace {
  unsigned pass_on(unsigned number) { return number; }
}

int main()
try {
  // spdlog::flush_on(spdlog::level::trace);

  constexpr auto max_events{100'000u};

  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {"job", max_events, 1u});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);

  g.provide("provide_number", [](data_cell_index const& id) -> unsigned { return id.number(); })
    .output_product("input", "number", "event");
  g.transform("pass_on", pass_on, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("different");
  g.execute();
} catch (std::exception const& e) {
  std::cerr << "Exception caught in main: " << e.what() << '\n';
  return 1;
} catch (...) {
  std::cerr << "Unknown exception caught in main.\n";
  return 1;
}

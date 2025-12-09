#include "phlex/core/framework_graph.hpp"
#include "phlex/model/product_store.hpp"

using namespace phlex::experimental;

namespace {
  unsigned pass_on(unsigned number) { return number; }
}

int main()
{
  constexpr auto max_events{100'000u};
  // constexpr auto max_events{1'000'000u};
  // spdlog::flush_on(spdlog::level::trace);

  auto cells_to_process = [](framework_driver& driver) {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);

    for (unsigned int i : std::views::iota(1u, max_events + 1)) {
      auto event_index = job_index->make_child(i, "event");
      driver.yield(event_index);
    }
  };

  framework_graph g{cells_to_process};
  g.provide("provide_number", [](data_cell_index const& id) -> unsigned { return id.number(); })
    .output_product("number"_in("event"));
  g.transform("pass_on", pass_on, concurrency::unlimited)
    .input_family("number"_in("event"))
    .output_products("different");
  g.execute();
}

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

using namespace phlex::experimental;

namespace {
  // Provider algorithms
 // int give_me_an_a(data_cell_index) { return 1; };
}

namespace {
  unsigned pass_on(unsigned number) { return number; }
}

TEST_CASE("provider_test")
{
  constexpr auto max_events{100'000u};
  // constexpr auto max_events{1'000'000u};
  // spdlog::flush_on(spdlog::level::trace);

  auto levels_to_process = [](framework_driver& driver) {
    auto job_store = product_store::base();
    driver.yield(job_store);

    for (unsigned int i : std::views::iota(1u, max_events + 1)) {
      auto store = job_store->make_child(i, "spill", "Source");
      store->add_product("number", i);
      driver.yield(store);
    }
  };

  framework_graph g{levels_to_process};

//  g.provider(give_me_an_a).input_indices("spill").output_product("Gauss:number");

  g.transform("pass_on", pass_on, concurrency::unlimited)
    .input_family("number"_in("spill"))
    .output_products("different");

  g.execute();
  CHECK(g.execution_counts("count") == 1);
}

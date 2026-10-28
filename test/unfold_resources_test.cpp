#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "plugins/layer_generator.hpp"

#include <catch2/catch_test_macros.hpp>

// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep
#include <utility>

using namespace phlex;
using namespace std::chrono_literals;

namespace {
  // Test load-bearing resource for unfold operations.
  struct unfold_resource {
    using token_type = unfold_resource*;

    detail::thread_counter::counter_type concurrent_unfolds;
  };

  class resource_iota {
  public:
    explicit resource_iota(unsigned int max_number) : max_{max_number} {}

    static unsigned int initial_value() { return 0; }
    bool predicate(unsigned int i) const { return i != max_; }

    static auto unfold(unsigned int i, unfold_resource* resource)
    {
      detail::thread_counter const guard{resource->concurrent_unfolds};
      detail::spin_for(1ms);
      return std::make_pair(i + 1, i);
    }

  private:
    unsigned int max_;
  };
}

TEST_CASE("Unfold receives a resource token", "[graph][unfold][resource]")
{
  constexpr auto num_events = 2u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = num_events});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_serialized_resource<unfold_resource>();

  g.provide(
     "provide_max_number", [](data_cell_index const&) { return 10u; }, concurrency::unlimited)
    .output_product("input", "max_number", "event");

  g.unfold<resource_iota>("resource_iota",
                          &resource_iota::predicate,
                          &resource_iota::unfold,
                          concurrency::unlimited,
                          "subevent")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "max_number"},
                  resource<unfold_resource>{})
    .output_product_suffixes("new_number");

  g.execute();

  CHECK(g.execution_count("resource_iota") == num_events);
}

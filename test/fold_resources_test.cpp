#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "plugins/layer_generator.hpp"
#include "test/fold_test_algorithms.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep
#include <cstddef>

using namespace phlex;
using namespace phlex::test;
using namespace std::chrono_literals;

TEST_CASE("Fold receives a resource token", "[graph][fold][resource]")
{
  constexpr auto number_runs = 2u;
  constexpr auto number_events = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = number_runs});
  gen->add_layer("event", {.parent_layer = "run", .count = number_events});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_serialized_resource<pointer_token_resource>();
  g.add_serialized_resource<second_pointer_token_resource>();

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  auto counter = detail::thread_counter::counter_type{};
  auto add_with_resources = [&counter](std::atomic<unsigned int>& sum,
                                       unsigned int number,
                                       pointer_token_resource const*,
                                       second_pointer_token_resource const*) {
    detail::thread_counter const throw_if_more_than_one_thread{counter};
    detail::spin_for(5ms);
    sum += number;
  };

  auto const input = product_selector{.creator = "input", .layer = "event", .suffix = "number"};
  g.fold("resource_add", add_with_resources, concurrency::unlimited, "run")
    .input_family(
      input, resource<pointer_token_resource>{}, resource<second_pointer_token_resource>{});
  g.observe("verify_resource_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family(product_selector{.creator = "resource_add", .layer = "run"});

  g.execute();

  CHECK(g.execution_count("resource_add") == std::size_t{number_runs} * number_events);
  CHECK(g.execution_count("verify_resource_sum") == number_runs);
}

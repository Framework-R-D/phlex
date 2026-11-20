#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;
using namespace std::chrono_literals;

TEST_CASE("registering a pointer resource with the graph", "[graph][resource]")
{
  auto g = framework_graph::with_default_driver("test");
  g.add_serialized_resource<pointer_token_resource>();

  g.provide(
     "number_maker", [](data_cell_index const&) { return 42; }, concurrency::unlimited)
    .output_product("number_maker", "", "job");

  auto counter = thread_counter::counter_type{};
  // Catch2 assertions are not thread-safe, so each observer records its
  // observation and the expectations are checked after the graph completes.
  std::atomic<unsigned int> expected_numbers_seen{};
  auto verify_number = [&counter, &expected_numbers_seen](int const num,
                                                          pointer_token_resource const*) {
    // Both observers share one resource token and must not run concurrently.
    thread_counter const throw_if_more_than_one_thread{counter};
    if (num == 42) {
      ++expected_numbers_seen;
    }
    spin_for(1ms);
  };

  product_selector const number_selector{.creator = "number_maker", .layer = "job"};
  g.observe("verify1", verify_number, concurrency::unlimited)
    .input_family(number_selector, resource<pointer_token_resource>{});
  g.observe("verify2", verify_number, concurrency::unlimited)
    .input_family(number_selector, resource<pointer_token_resource>{});

  g.execute();

  CHECK(g.execution_count("verify1") == 1);
  CHECK(g.execution_count("verify2") == 1);
  // One observation per observer, each of the expected number.
  CHECK(expected_numbers_seen == 2u);
}

// =======================================================================================
/*
   This test executes the following graph

              Index Router
              /        \
             /          \
        job_add(*)     run_add(^)
            |             |\
            |             | \
            |             |  \
     verify_job_sum       |   \
                          |    \
               verify_run_sum   \
                                 \
                             two_layer_job_add(*)
                                  |
                                  |
                           verify_two_layer_job_sum

   where the asterisk (*) indicates a fold step over the full job, and the caret (^)
   represents a fold step over each run.
*/
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <numeric>
#include <string>
#include <vector>

using namespace phlex;
using namespace std::chrono_literals;

namespace {
  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  void collect(std::vector<unsigned int>& numbers, unsigned int number)
  {
    numbers.push_back(number);
  }

  void verify_collected_numbers(std::vector<unsigned int> const& actual)
  {
    CHECK(actual.size() == 5u);
    auto const sum = std::accumulate(actual.begin(), actual.end(), 0u);
    CHECK(sum == 10u);
  }

  // Provider algorithm
  unsigned int provide_number(data_cell_index const& id) { return id.number(); }

  struct fold_resource_1 {
    using token_type = fold_resource_1 const*;
  };

  struct fold_resource_2 {
    using token_type = fold_resource_2 const*;
  };
}

TEST_CASE("Different data layers of fold", "[graph]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = index_limit});
  gen->add_layer("event", {.parent_layer = "run", .count = number_limit});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  g.fold("run_add", add, concurrency::unlimited, "run")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("run_sum");
  g.fold("job_add", add, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("job_sum");

  g.fold("two_layer_job_add", add, concurrency::unlimited)
    .input_family(product_selector{.creator = "run_add", .layer = "run", .suffix = "run_sum"})
    .output_product_suffixes("two_layer_job_sum");

  g.observe("verify_run_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family(product_selector{.creator = "run_add", .layer = "run", .suffix = "run_sum"});
  g.observe("verify_two_layer_job_sum", [](unsigned int actual) { CHECK(actual == 20u); })
    .input_family(product_selector{
      .creator = "two_layer_job_add", .layer = "job", .suffix = "two_layer_job_sum"});
  g.observe("verify_job_sum", [](unsigned int actual) { CHECK(actual == 20u); })
    .input_family(product_selector{.creator = "job_add", .layer = "job", .suffix = "job_sum"});

  g.execute();

  CHECK(g.execution_count("run_add") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("job_add") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("two_layer_job_add") == index_limit);
  CHECK(g.execution_count("verify_run_sum") == index_limit);
  CHECK(g.execution_count("verify_two_layer_job_sum") == 1);
  CHECK(g.execution_count("verify_job_sum") == 1);
}

TEST_CASE("Fold output without send consumed downstream", "[graph]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = index_limit});
  gen->add_layer("event", {.parent_layer = "run", .count = number_limit});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  g.fold("collect_numbers", collect, concurrency::serial, "run")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("numbers");

  g.observe("verify_collected_numbers", verify_collected_numbers, concurrency::serial)
    .input_family(
      product_selector{.creator = "collect_numbers", .layer = "run", .suffix = "numbers"});

  g.execute();

  CHECK(g.execution_count("collect_numbers") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("verify_collected_numbers") == index_limit);
}

TEST_CASE("Fold receives a resource token", "[graph][fold][resource]")
{
  constexpr auto number_runs = 2u;
  constexpr auto number_events = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = number_runs});
  gen->add_layer("event", {.parent_layer = "run", .count = number_events});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);
  g.add_resource<fold_resource_1>();
  g.add_resource<fold_resource_2>();

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  auto counter = detail::thread_counter::counter_type{};
  auto add_with_resources = [&counter](std::atomic<unsigned int>& sum,
                                       unsigned int number,
                                       fold_resource_1 const*,
                                       fold_resource_2 const*) {
    detail::thread_counter throw_if_more_than_one_thread{counter};
    detail::spin_for(5ms);
    sum += number;
  };

  auto const input = product_selector{.creator = "input", .layer = "event", .suffix = "number"};
  g.fold("resource_add", add_with_resources, concurrency::unlimited, "run")
    .input_family(input, resource<fold_resource_1>{}, resource<fold_resource_2>{});
  g.observe("verify_resource_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family(product_selector{.creator = "resource_add", .layer = "run"});

  g.execute();

  CHECK(g.execution_count("resource_add") == std::size_t{number_runs} * number_events);
  CHECK(g.execution_count("verify_resource_sum") == number_runs);
}

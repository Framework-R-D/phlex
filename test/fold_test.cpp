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
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "plugins/layer_generator.hpp"
#include "test/ostream_logger.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep
#include <cstddef>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace phlex;
using namespace std::chrono_literals;

namespace {
  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  /// Accumulates the sum and count of `number` values into the two elements of `result`.
  /// `result` must remain valid for the duration of the call, and updates to both atomic
  /// counters are safe under concurrent fold execution.
  void add_and_count(std::tuple<std::atomic<unsigned int>, std::atomic<unsigned int>>& result,
                     unsigned int number)
  {
    auto& [sum, count] = result;
    sum += number;
    ++count;
  }

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

  auto g = phlex::detail::framework_graph::without_driver("test");
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

TEST_CASE("Multi-layer fold with parent and child inputs", "[graph][fold]")
{
  auto const [first, second] = GENERATE(
    table<std::string_view, std::string_view>({{"event", "subevent"}, {"subevent", "event"}}));
  CAPTURE(first, second);

  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  {
    std::vector<unsigned int> sums;
    constexpr auto number_events = 2u;
    constexpr auto number_subevents = 10u;

    auto gen = experimental::layer_generator::make();
    gen->add_layer("event", {.parent_layer = "job", .count = number_events});
    gen->add_layer("subevent", {.parent_layer = "event", .count = number_subevents});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);
    g.provide(
       "provide_parent_number",
       [](data_cell_index const& id) -> unsigned int { return 10u * (id.number() + 1); },
       concurrency::unlimited)
      .output_product("input", "number", "event");
    g.provide("provide_child_number", provide_number, concurrency::unlimited)
      .output_product("input", "number", "subevent");

    // The accumulator is first. Bookkeeping must count subevents in either input order.
    g.fold(
       "add_parent_and_child",
       [](unsigned int& sum, unsigned int first, unsigned int second) { sum += first + second; },
       concurrency::serial,
       "event")
      .input_family(product_selector{.layer = first}, product_selector{.layer = second});

    g.observe(
       "check_parent_and_child_sum",
       [&sums](unsigned int sum) { sums.push_back(sum); },
       concurrency::serial)
      .input_family({.creator = "add_parent_and_child", .layer = "event"});

    g.execute();

    CHECK(g.execution_count("provide_parent_number") == number_events);
    CHECK(g.execution_count("provide_child_number") ==
          std::size_t{number_events} * number_subevents);
    CHECK(g.execution_count("add_parent_and_child") ==
          std::size_t{number_events} * number_subevents);
    CHECK(g.execution_count("check_parent_and_child_sum") == number_events);

    std::ranges::sort(sums);
    CHECK(sums == std::vector<unsigned int>{145u, 245u});
  }
  CHECK_FALSE(output.str().contains("Cached"));
}

TEST_CASE("Multi-layer fold excludes products outside its partition", "[graph][fold]")
{
  auto const [first, second] = GENERATE(
    table<std::string_view, std::string_view>({{"event", "subevent"}, {"subevent", "event"}}));
  CAPTURE(first, second);

  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  {
    std::vector<unsigned int> sums;
    constexpr auto number_subevents = 10u;

    auto gen = experimental::layer_generator::make();
    gen->add_layer("run", {.parent_layer = "job", .count = 1});
    gen->add_layer("event", {.parent_layer = "run", .count = 2});
    gen->add_layer("event", {.parent_layer = "job", .count = 1});
    gen->add_layer("subevent", {.parent_layer = "/job/run/event", .count = number_subevents});
    gen->add_layer("subevent", {.parent_layer = "/job/event", .count = number_subevents});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);
    g.provide(
       "provide_parent_number",
       [](data_cell_index const& id) -> unsigned int { return 10u * (id.number() + 1); },
       concurrency::unlimited)
      .output_product("input", "number", "event");
    g.provide("provide_child_number", provide_number, concurrency::unlimited)
      .output_product("input", "number", "subevent");
    g.fold(
       "add_parent_and_child",
       [](unsigned int& sum, unsigned int first, unsigned int second) { sum += first + second; },
       concurrency::serial,
       "run")
      .input_family(product_selector{.layer = first}, product_selector{.layer = second});
    g.observe(
       "check_parent_and_child_sum",
       [&sums](unsigned int sum) { sums.push_back(sum); },
       concurrency::serial)
      .input_family({.creator = "add_parent_and_child", .layer = "run"});

    g.execute();

    CHECK(g.execution_count("provide_parent_number") == 3uz);
    CHECK(g.execution_count("provide_child_number") == 3uz * number_subevents);
    CHECK(g.execution_count("add_parent_and_child") == 2uz * number_subevents);
    CHECK(g.execution_count("check_parent_and_child_sum") == 1uz);
    CHECK(sums == std::vector<unsigned int>{390u});
  }
  CHECK_FALSE(output.str().contains("Cached"));
}

TEST_CASE("Fold output without send consumed downstream", "[graph]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = index_limit});
  gen->add_layer("event", {.parent_layer = "run", .count = number_limit});

  auto g = phlex::detail::framework_graph::without_driver("test");
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

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_serialized_resource<fold_resource_1>();
  g.add_serialized_resource<fold_resource_2>();

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  auto counter = detail::thread_counter::counter_type{};
  auto add_with_resources = [&counter](std::atomic<unsigned int>& sum,
                                       unsigned int number,
                                       fold_resource_1 const*,
                                       fold_resource_2 const*) {
    detail::thread_counter const throw_if_more_than_one_thread{counter};
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

TEST_CASE("Multi-output fold", "[graph][fold]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {.parent_layer = "job", .count = index_limit});
  gen->add_layer("event", {.parent_layer = "run", .count = number_limit});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("input", "number", "event");

  g.fold("run_add_and_count", add_and_count, concurrency::unlimited, "run")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("run_sum", "run_count");

  g.observe("verify_run_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family(
      product_selector{.creator = "run_add_and_count", .layer = "run", .suffix = "run_sum"});
  g.observe("verify_run_count", [](unsigned int actual) { CHECK(actual == 5u); })
    .input_family(
      product_selector{.creator = "run_add_and_count", .layer = "run", .suffix = "run_count"});

  g.execute();

  CHECK(g.execution_count("run_add_and_count") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("verify_run_sum") == index_limit);
  CHECK(g.execution_count("verify_run_count") == index_limit);
}

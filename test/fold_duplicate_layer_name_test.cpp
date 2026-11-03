// =======================================================================================
// This test executes the following graph
//
//        Index Router
//        |         |
//    job_add(*) run_add(^)
//        |         |
//        |     verify_run_sum
//        |
//   verify_job_sum
//
// where the asterisk (*) indicates a fold step over the full job, and the caret (^)
// represents a fold step over each run.
//
// The hierarchy tested is:
//
//    job
//     │
//     ├ event
//     │
//     └ run
//        │
//        └ event
//
// As the run_add node performs folds only over "runs", any top-level "events"
// stores are excluded from the fold result.
//
// N.B. The index_router sends data products to nodes based on the name of the lowest
//      layer.  For example, the top-level "event" and the nested "run/event" are both
//      candidates for the "job" fold.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"
#include "test/ostream_logger.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace phlex;

namespace {
  // Provider function
  unsigned int provide_number(data_cell_index const& index) { return index.number(); }

  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  class split {
  public:
    explicit split(unsigned int count) : count_{count} {}
    static unsigned int initial_value() { return 0; }
    bool predicate(unsigned int i) const { return i < count_; }
    static auto unfold(unsigned int i) { return std::make_pair(i + 1, i + 1); }

  private:
    unsigned int count_;
  };

  void check_no_cached_entries(std::ostringstream const& output)
  {
    auto const messages = output.str();
    CHECK_FALSE(messages.contains("Cached"));
  }
}

TEST_CASE("Fold different layer paths with same trailing name", "[graph]")
{
  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [] {
    std::vector<unsigned int> run_sums;
    std::vector<unsigned int> job_sums;
    // job -> run -> event layers
    constexpr auto number_runs = 2u;
    constexpr auto number_events_per_run = 5u;

    // job -> event layers
    constexpr auto number_top_level_events = 10u;

    auto gen = experimental::layer_generator::make();
    gen->add_layer("run", {.parent_layer = "job", .count = number_runs});
    gen->add_layer("event", {.parent_layer = "run", .count = number_events_per_run});
    gen->add_layer("event", {.parent_layer = "job", .count = number_top_level_events});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);

    // Register provider
    g.provide("provide_number", provide_number, concurrency::unlimited)
      .output_product("input", "number", "event");

    g.fold("run_add", add, concurrency::unlimited, "run", 0u)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
      .output_product_suffixes("run_sum");
    g.fold("job_add", add, concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
      .output_product_suffixes("job_sum");

    g.observe(
       "verify_run_sum",
       [&run_sums](unsigned int actual) { run_sums.push_back(actual); },
       concurrency::serial)
      .input_family(product_selector{.creator = "run_add", .layer = "run", .suffix = "run_sum"});
    g.observe(
       "verify_job_sum",
       [&job_sums](unsigned int actual) { job_sums.push_back(actual); },
       concurrency::serial)
      .input_family(product_selector{.creator = "job_add", .layer = "job", .suffix = "job_sum"});

    g.execute();

    CHECK(g.execution_count("run_add") == std::size_t{number_runs} * number_events_per_run);
    CHECK(g.execution_count("job_add") ==
          (number_runs * number_events_per_run) + number_top_level_events);
    CHECK(g.execution_count("verify_run_sum") == number_runs);
    CHECK(g.execution_count("verify_job_sum") == 1);
    return std::make_pair(std::move(run_sums), std::move(job_sums));
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto [run_sums, job_sums] = run_graph();
  std::ranges::sort(run_sums);
  CHECK(run_sums == std::vector<unsigned int>{10, 10});
  CHECK(job_sums == std::vector<unsigned int>{65});
  check_no_cached_entries(output);
}

TEST_CASE("Empty run fold excludes foreign pass-through event products", "[graph][fold]")
{
  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  {
    constexpr auto foreign_count = 3u;
    std::vector<unsigned int> sums;
    auto gen = experimental::layer_generator::make();
    gen->add_layer("run", {.parent_layer = "job", .count = 1});
    // Declare the eligible invocation path without generating any eligible events.
    gen->add_layer("event", {.parent_layer = "run", .count = 0});
    gen->add_layer("event", {.parent_layer = "job", .count = foreign_count});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);
    g.provide("provide_number", provide_number, concurrency::unlimited)
      .output_product("input", "number", "event");
    g.fold("run_add", add, concurrency::unlimited, "run", 0u)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
      .output_product_suffixes("run_sum");
    g.observe(
       "collect_sum", [&sums](unsigned int sum) { sums.push_back(sum); }, concurrency::serial)
      .input_family(product_selector{.creator = "run_add", .layer = "run", .suffix = "run_sum"});

    g.execute();

    CHECK(gen->emitted_cell_count("/job/run") == 1);
    CHECK(gen->emitted_cell_count("/job/run/event") == 0);
    CHECK(gen->emitted_cell_count("/job/event") == foreign_count);
    CHECK(g.execution_count("provide_number") == foreign_count);
    CHECK(g.execution_count("run_add") == 0);
    CHECK(g.execution_count("collect_sum") == 1);
    CHECK(sums == std::vector<unsigned int>{0u});
  }
  // Unmatched foreign products must not remain cached after the graph is destroyed.
  INFO(output.str());
  CHECK_FALSE(output.str().contains("[run_add/event] Cached messages"));
  check_no_cached_entries(output);
}

TEST_CASE("Fold duplicate event paths with empty unfolds", "[graph][issue955]")
{
  auto const [description, top_count, run_count] =
    GENERATE(Catch::Generators::table<char const*, unsigned int, unsigned int>(
      {{"Both paths populated", 3u, 2u},
       {"Empty top branch events", 0u, 2u},
       {"Empty nested events", 3u, 0u},
       {"Both paths empty", 0u, 0u}}));
  CAPTURE(description, top_count, run_count);

  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [](unsigned int top_count, unsigned int run_count) {
    std::vector<unsigned int> sums;
    auto gen = experimental::layer_generator::make();
    gen->add_layer("top", {.parent_layer = "job", .count = 1});
    gen->add_layer("run", {.parent_layer = "job", .count = 2});
    auto g = phlex::detail::framework_graph::without_driver("test", 1);
    g.add_driver(gen);
    g.provide(
       "top_count",
       [top_count](data_cell_index const&) { return top_count; },
       concurrency::unlimited)
      .output_product("input", "top_count", "top");
    g.provide(
       "run_count",
       [run_count](data_cell_index const&) { return run_count; },
       concurrency::unlimited)
      .output_product("input", "run_count", "run");
    g.unfold<split>(
       "top_events", &split::predicate, &split::unfold, concurrency::unlimited, "event")
      .input_family(product_selector{.creator = "input", .layer = "top", .suffix = "top_count"})
      .output_product_suffixes("top_seed");
    g.unfold<split>(
       "run_events", &split::predicate, &split::unfold, concurrency::unlimited, "event")
      .input_family(product_selector{.creator = "input", .layer = "run", .suffix = "run_count"})
      .output_product_suffixes("nested_seed");
    g.provide(
       "event_number",
       [](data_cell_index const& index) { return static_cast<unsigned int>(index.number() + 1); },
       concurrency::unlimited)
      .output_product("input", "number", "event");
    g.fold("job_add", add, concurrency::unlimited, "job", 0u)
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"})
      .output_product_suffixes("sum");
    g.observe(
       "collect_sum", [&sums](unsigned int sum) { sums.push_back(sum); }, concurrency::serial)
      .input_family(product_selector{.creator = "job_add", .layer = "job", .suffix = "sum"});
    g.execute();

    CHECK(g.execution_count("top_events") == 1);
    CHECK(g.execution_count("run_events") == 2);
    CHECK(g.execution_count("job_add") == top_count + (2 * run_count));
    CHECK(g.execution_count("collect_sum") == 1);
    return sums;
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto const sums = run_graph(top_count, run_count);
  auto const expected = (top_count * (top_count + 1) / 2) + (run_count * (run_count + 1));
  CHECK(sums == std::vector<unsigned int>{expected});
  check_no_cached_entries(output);
}

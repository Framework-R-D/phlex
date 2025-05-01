// =======================================================================================
/*
   This test executes the following graph

              Multiplexer
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
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_all.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <ranges>
#include <string>
#include <vector>

using namespace phlex::experimental;

namespace {
  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }
}

TEST_CASE("Different levels of fold", "[graph]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  auto levels_to_process = [index_limit, number_limit](framework_driver& driver) {
    auto job_store = product_store::base();
    driver.yield(job_store);
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_store = job_store->make_child(i, "run");
      driver.yield(run_store);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        auto event_store = run_store->make_child(j, "event");
        event_store->add_product("number", j);
        driver.yield(event_store);
      }
    }
  };

  // framework_graph g{levels_to_process};
  framework_graph g{levels_to_process};

  g.with("run_add", add, concurrency::unlimited).fold("number").partitioned_by("run").to("run_sum");
  g.with("job_add", add, concurrency::unlimited).fold("number").to("job_sum");

  g.with("two_layer_job_add", add, concurrency::unlimited).fold("run_sum").to("two_layer_job_sum");

  g.with(
     "verify_run_sum", [](unsigned int actual) { CHECK(actual == 10u); }, concurrency::unlimited)
    .observe("run_sum");
  g.with(
     "verify_two_layer_job_sum",
     [](unsigned int actual) { CHECK(actual == 20u); },
     concurrency::unlimited)
    .observe("two_layer_job_sum");
  g.with(
     "verify_job_sum", [](unsigned int actual) { CHECK(actual == 20u); }, concurrency::unlimited)
    .observe("job_sum");

  g.execute();

  CHECK(g.execution_counts("run_add") == index_limit * number_limit);
  CHECK(g.execution_counts("job_add") == index_limit * number_limit);
  CHECK(g.execution_counts("two_layer_job_add") == index_limit);
  CHECK(g.execution_counts("verify_run_sum") == index_limit);
  CHECK(g.execution_counts("verify_two_layer_job_sum") == 1);
  CHECK(g.execution_counts("verify_job_sum") == 1);
}

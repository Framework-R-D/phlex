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
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <string>

using namespace phlex;

namespace {
  void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  // Provider algorithm
  unsigned int provide_number(data_cell_index const& id) { return id.number(); }
}

TEST_CASE("Different data layers of fold", "[graph]")
{
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  experimental::layer_generator gen;
  gen.add_layer("run", {"job", index_limit});
  gen.add_layer("event", {"run", number_limit});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_number", provide_number, concurrency::unlimited)
    .output_product("number"_in("event"));

  g.fold("run_add", add, concurrency::unlimited, "run")
    .input_family("number"_in("event"))
    .output_products("run_sum");
  g.fold("job_add", add, concurrency::unlimited)
    .input_family("number"_in("event"))
    .output_products("job_sum");

  g.fold("two_layer_job_add", add, concurrency::unlimited)
    .input_family("run_sum"_in("run"))
    .output_products("two_layer_job_sum");

  g.observe("verify_run_sum", [](unsigned int actual) { CHECK(actual == 10u); })
    .input_family("run_sum"_in("run"));
  g.observe("verify_two_layer_job_sum", [](unsigned int actual) { CHECK(actual == 20u); })
    .input_family("two_layer_job_sum"_in("job"));
  g.observe("verify_job_sum", [](unsigned int actual) { CHECK(actual == 20u); })
    .input_family("job_sum"_in("job"));

  g.execute();

  CHECK(g.execution_count("run_add") == index_limit * number_limit);
  CHECK(g.execution_count("job_add") == index_limit * number_limit);
  CHECK(g.execution_count("two_layer_job_add") == index_limit);
  CHECK(g.execution_count("verify_run_sum") == index_limit);
  CHECK(g.execution_count("verify_two_layer_job_sum") == 1);
  CHECK(g.execution_count("verify_job_sum") == 1);
}

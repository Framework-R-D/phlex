// =======================================================================================
// This test executes the following graph
//
//     Multiplexer
//      |       |
//   get_time square
//      |       |
//      |      add(*)
//      |       |
//      |     scale
//      |       |
//     print_result [also includes output module]
//
// where the asterisk (*) indicates a fold step.  In terms of the data model,
// whenever the add node receives the flush token, a product is inserted at one level
// higher than the level processed by square and add nodes.
// =======================================================================================

#include "phlex/core/fold/send.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_test_macros.hpp"
#include "spdlog/spdlog.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <ctime>
#include <ranges>
#include <string>

using namespace phlex::experimental;

namespace {
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  void levels_to_process(framework_driver& driver)
  {
    auto job_store = product_store::base();
    driver.yield(job_store);
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_store = job_store->make_child(i, "run", "levels_to_process");
      run_store->add_product<std::time_t>("time", std::time(nullptr));
      driver.yield(run_store);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        auto event_store = run_store->make_child(j, "event", "levels_to_process");
        event_store->add_product("number", i + j);
        driver.yield(event_store);
      }
    }
  }

  auto square(unsigned int const num) { return num * num; }

  struct data_for_rms {
    unsigned int total;
    unsigned int number;
  };

  struct threadsafe_data_for_rms {
    std::atomic<unsigned int> total;
    std::atomic<unsigned int> number;
  };

  auto send(threadsafe_data_for_rms const& data) -> data_for_rms
  {
    return {.total=phlex::experimental::send(data.total), .number=phlex::experimental::send(data.number)};
  }

  void add(threadsafe_data_for_rms& redata, unsigned squared_number)
  {
    redata.total += squared_number;
    ++redata.number;
  }

  auto scale(data_for_rms data) -> double
  {
    return std::sqrt(static_cast<double>(data.total) / data.number);
  }

  auto strtime(std::time_t tm) -> std::string
  {
    char buffer[32];
    std::strncpy(buffer, std::ctime(&tm), 26);
    return buffer;
  }

  void print_result(handle<double> result, std::string const& stringized_time)
  {
    spdlog::debug("{}: {} @ {}",
                  result.level_id().to_string(),
                  *result,
                  stringized_time.substr(0, stringized_time.find('\n')));
  }
}

TEST_CASE("Hierarchical nodes", "[graph]")
{
  framework_graph g{levels_to_process};

  g.transform("get_the_time", strtime, concurrency::unlimited)
    .input_family("time")
    .when()
    .output_products("strtime");
  g.transform("square", square, concurrency::unlimited)
    .input_family("number")
    .output_products("squared_number");

  g.fold("add", add, concurrency::unlimited, "run", 15u)
    .input_family("squared_number")
    .when()
    .output_products("added_data");

  g.transform("scale", scale, concurrency::unlimited)
    .input_family("added_data")
    .output_products("result");
  g.observe("print_result", print_result, concurrency::unlimited).input_family("result", "strtime");

  g.make<test::products_for_output>().output("save", &test::products_for_output::save).when();

  g.execute();

  CHECK(g.execution_counts("square") == index_limit * number_limit);
  CHECK(g.execution_counts("add") == index_limit * number_limit);
  CHECK(g.execution_counts("get_the_time") >= index_limit);
  CHECK(g.execution_counts("scale") == index_limit);
  CHECK(g.execution_counts("print_result") == index_limit);
}

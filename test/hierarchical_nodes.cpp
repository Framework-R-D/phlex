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

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_all.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <cmath>
#include <ctime>
#include <ranges>
#include <string>
#include <vector>

using namespace phlex::experimental;

namespace {
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

  void levels_to_process(framework_driver& driver)
  {
    auto job_store = product_store::base();
    driver.yield(job_store);
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_store = job_store->make_child(i, "run");
      run_store->add_product<std::time_t>("time", std::time(nullptr));
      driver.yield(run_store);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        auto event_store = run_store->make_child(j, "event");
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

  data_for_rms send(threadsafe_data_for_rms const& data)
  {
    return {phlex::experimental::send(data.total), phlex::experimental::send(data.number)};
  }

  void add(threadsafe_data_for_rms& redata, unsigned squared_number)
  {
    redata.total += squared_number;
    ++redata.number;
  }

  double scale(data_for_rms data)
  {
    return std::sqrt(static_cast<double>(data.total) / data.number);
  }

  std::string strtime(std::time_t tm)
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

  g.with("get_the_time", strtime, concurrency::unlimited).when().transform("time").to("strtime");
  g.with(square, concurrency::unlimited).transform("number").to("squared_number");

  g.with("add", add, concurrency::unlimited)
    .when()
    .fold("squared_number")
    .partitioned_by("run")
    .to("added_data")
    .initialized_with(15u);

  g.with(scale, concurrency::unlimited).transform("added_data").to("result");
  g.with(print_result, concurrency::unlimited).observe("result", "strtime");

  g.make<test::products_for_output>().output_with(&test::products_for_output::save).when();

  g.execute("hierarchical_nodes_t");

  CHECK(g.execution_counts("square") == index_limit * number_limit);
  CHECK(g.execution_counts("add") == index_limit * number_limit);
  CHECK(g.execution_counts("get_the_time") >= index_limit);
  CHECK(g.execution_counts("scale") == index_limit);
  CHECK(g.execution_counts("print_result") == index_limit);
}

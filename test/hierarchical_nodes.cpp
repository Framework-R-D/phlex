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
// whenever the add node receives the flush token, a product is inserted in one data layer
// higher than the data layer processed by square and add nodes.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_test_macros.hpp"
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

  void cells_to_process(framework_driver& driver)
  {
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);
    for (unsigned i : std::views::iota(0u, index_limit)) {
      auto run_index = job_index->make_child(i, "run");
      driver.yield(run_index);
      for (unsigned j : std::views::iota(0u, number_limit)) {
        driver.yield(run_index->make_child(j, "event"));
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
                  result.data_cell_index().to_string(),
                  *result,
                  stringized_time.substr(0, stringized_time.find('\n')));
  }
}

TEST_CASE("Hierarchical nodes", "[graph]")
{
  framework_graph g{cells_to_process};

  g.provide("provide_time",
            [](data_cell_index const& index) -> std::time_t {
              spdlog::info("Providing time for {}", index.to_string());
              return std::time(nullptr);
            })
    .output_product("time"_in("run"));

  g.provide("provide_number",
            [](data_cell_index const& index) -> unsigned int {
              auto const event_number = index.number();
              auto const run_number = index.parent()->number();
              return event_number + run_number;
            })
    .output_product("number"_in("event"));

  g.transform("get_the_time", strtime, concurrency::unlimited)
    .input_family("time"_in("run"))
    .when()
    .output_products("strtime");
  g.transform("square", square, concurrency::unlimited)
    .input_family("number"_in("event"))
    .output_products("squared_number");

  g.fold("add", add, concurrency::unlimited, "run", 15u)
    .input_family("squared_number"_in("event"))
    .when()
    .output_products("added_data");

  g.transform("scale", scale, concurrency::unlimited)
    .input_family("added_data"_in("run"))
    .output_products("result");
  g.observe("print_result", print_result, concurrency::unlimited)
    .input_family("result"_in("run"), "strtime"_in("run"));

  g.make<test::products_for_output>().output("save", &test::products_for_output::save).when();

  try {
    g.execute();
  } catch (std::exception const& e) {
    spdlog::error(e.what());
  }

  CHECK(g.execution_counts("square") == index_limit * number_limit);
  CHECK(g.execution_counts("add") == index_limit * number_limit);
  CHECK(g.execution_counts("get_the_time") >= index_limit);
  CHECK(g.execution_counts("scale") == index_limit);
  CHECK(g.execution_counts("print_result") == index_limit);
}

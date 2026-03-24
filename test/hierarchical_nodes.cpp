// =======================================================================================
// This test executes the following graph
//
//     Index Router
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
#include "plugins/layer_generator.hpp"
#include "test/products_for_output.hpp"

#include "catch2/catch_test_macros.hpp"
#include "fmt/chrono.h"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>

using namespace phlex;

namespace {
  constexpr auto index_limit = 2u;
  constexpr auto number_limit = 5u;

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

  std::string strtime(std::chrono::system_clock::time_point tp)
  {
    return fmt::format("{:%a %b %d %H:%M:%S %Y}", tp);
  }

  void print_result(handle<double> result, std::string const& stringized_time)
  {
    spdlog::debug("{}: {} @ {}", result.data_cell_index().to_string(), *result, stringized_time);
  }
}

TEST_CASE("Hierarchical nodes", "[graph]")
{
  experimental::layer_generator gen;
  gen.add_layer("run", {"job", index_limit});
  gen.add_layer("event", {"run", number_limit});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_time",
            [](data_cell_index const& index) {
              spdlog::info("Providing time for {}", index.to_string());
              return std::chrono::system_clock::now();
            })
    .output_product(product_query{.creator = "input", .layer = "run", .suffix = "time"});

  g.provide("provide_number",
            [](data_cell_index const& index) -> unsigned int {
              auto const event_number = index.number();
              auto const run_number = index.parent()->number();
              return event_number + run_number;
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "number"});

  g.transform("get_the_time", strtime, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "run", .suffix = "time"})
    .experimental_when()
    .output_product_suffixes("strtime");
  g.transform("square", square, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "number"})
    .output_product_suffixes("squared_number");

  g.fold("add", add, concurrency::unlimited, "run", 15u)
    .input_family(product_query{.creator = "square", .layer = "event", .suffix = "squared_number"})
    .experimental_when()
    .output_product_suffixes("added_data");

  g.transform("scale", scale, concurrency::unlimited)
    .input_family(product_query{.creator = "add", .layer = "run", .suffix = "added_data"})
    .output_product_suffixes("result");
  g.observe("print_result", print_result, concurrency::unlimited)
    .input_family(product_query{.creator = "scale", .layer = "run", .suffix = "result"},
                  product_query{.creator = "get_the_time", .layer = "run", .suffix = "strtime"});

  g.make<experimental::test::products_for_output>()
    .output("save", &experimental::test::products_for_output::save)
    .experimental_when();

  try {
    g.execute();
  } catch (std::exception const& e) {
    spdlog::error(e.what());
  }

  CHECK(g.execution_count("square") == index_limit * number_limit);
  CHECK(g.execution_count("add") == index_limit * number_limit);
  CHECK(g.execution_count("get_the_time") == index_limit);
  CHECK(g.execution_count("scale") == index_limit);
  CHECK(g.execution_count("print_result") == index_limit);
}

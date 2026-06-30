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
    return {experimental::send(data.total), experimental::send(data.number)};
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
  auto gen = experimental::layer_generator::make();
  gen->add_layer("run", {"job", index_limit});
  gen->add_layer("event", {"run", number_limit});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);

  g.provide("provide_time",
            [](data_cell_index const& index) {
              spdlog::info("Providing time for {}", index.to_string());
              return std::chrono::system_clock::now();
            })
    .output_product("input", "time", "run");

  g.provide("provide_number",
            [](data_cell_index const& index) -> unsigned int {
              auto const event_number = index.number();
              auto const run_number = index.parent()->number();
              return event_number + run_number;
            })
    .output_product("input", "number", "event");

  g.transform("get_the_time", strtime, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "run", .suffix = "time"})
    .experimental_when();
  g.transform("square", square, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "number"});

  g.fold("add", add, concurrency::unlimited, "run", 15u)
    .input_family(product_selector{.creator = "square", .layer = "event"})
    .experimental_when();

  g.transform("scale", scale, concurrency::unlimited)
    .input_family(product_selector{.creator = "add", .layer = "run"});
  g.observe("print_result", print_result, concurrency::unlimited)
    .input_family(product_selector{.creator = "scale", .layer = "run"},
                  product_selector{.creator = "get_the_time", .layer = "run"});

  g.make<experimental::test::products_for_output>()
    .output("save", &experimental::test::products_for_output::save)
    .experimental_when();

  g.execute();

  CHECK(g.execution_count("square") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("add") == std::size_t{index_limit} * number_limit);
  CHECK(g.execution_count("get_the_time") == index_limit);
  CHECK(g.execution_count("scale") == index_limit);
  CHECK(g.execution_count("print_result") == index_limit);
}

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
#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <cmath>
#include <ctime>
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
  experimental::layer_generator gen;
  gen.add_layer("run", {"job", index_limit});
  gen.add_layer("event", {"run", number_limit});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_time",
            [](data_cell_index const& index) -> std::time_t {
              spdlog::info("Providing time for {}", index.to_string());
              return std::time(nullptr);
            })
    .output_product(product_query{.creator = "input"_id, .layer = "run"_id, .suffix = "time"_id});

  g.provide("provide_number",
            [](data_cell_index const& index) -> unsigned int {
              auto const event_number = index.number();
              auto const run_number = index.parent()->number();
              return event_number + run_number;
            })
    .output_product(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "number"_id});

  g.transform("get_the_time", strtime, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "run"_id, .suffix = "time"_id})
    .experimental_when()
    .output_products("strtime");
  g.transform("square", square, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "number"_id})
    .output_products("squared_number");

  g.fold("add", add, concurrency::unlimited, "run", 15u)
    .input_family(
      product_query{.creator = "square"_id, .layer = "event"_id, .suffix = "squared_number"_id})
    .experimental_when()
    .output_products("added_data");

  g.transform("scale", scale, concurrency::unlimited)
    .input_family(product_query{.creator = "add"_id, .layer = "run"_id, .suffix = "added_data"_id})
    .output_products("result");
  g.observe("print_result", print_result, concurrency::unlimited)
    .input_family(
      product_query{.creator = "scale"_id, .layer = "run"_id, .suffix = "result"_id},
      product_query{.creator = "get_the_time"_id, .layer = "run"_id, .suffix = "strtime"_id});

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

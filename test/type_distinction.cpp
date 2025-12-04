#include "phlex/core/framework_graph.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include "catch2/catch_test_macros.hpp"

#include <tuple>
#include <vector>

using namespace phlex::experimental;

namespace {
  auto add_numbers(int x, int y) { return x + y; }

  auto triple(int x) { return 3 * x; }

  auto square(int x) { return std::tuple{x * x, double((x * x) + 0.5)}; }

  int id(int x) { return x; }

  auto add_vectors(std::vector<int> const& x, std::vector<int> const& y)
  {
    std::vector<int> res;
    std::size_t const len = std::min(x.size(), y.size());

    res.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
      res.push_back(x[i] + y[i]);
    }
    return res;
  }

  auto expand(int x, std::size_t len) { return std::vector<int>(len, x); }
}

TEST_CASE("Distinguish products with same name and different types", "[programming model]")
{

  auto gen = [](auto& driver) {
    std::vector<int> numbers{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto job_store = product_store::base();
    driver.yield(job_store);
    for (int i : numbers) {
      auto event_store = job_store->make_child(unsigned(i), "event");
      event_store->add_product<int>("numbers", +i);
      event_store->add_product<std::size_t>("length", +i);
      driver.yield(event_store);
    }
  };

  framework_graph g{gen};

  SECTION("Duplicate product name but differ in producer name")
  {
    g.observe("starter", [](int num) { spdlog::info("Recieved {}", num); }).input_family("numbers");
    g.transform("triple_numbers", triple, concurrency::unlimited)
      .input_family("numbers")
      .output_products("tripled");
    spdlog::info("Registered tripled");
    g.transform("expand_orig", expand, concurrency::unlimited)
      .input_family("numbers", "length")
      .output_products("expanded_one");
    spdlog::info("Registered expanded_one");
    g.transform("expand_triples", expand, concurrency::unlimited)
      .input_family("tripled", "length")
      .output_products("expanded_three");
    spdlog::info("Registered expanded_three");

    g.transform("add_nums", add_numbers, concurrency::unlimited)
      .input_family("numbers", "tripled")
      .output_products("sums");
    spdlog::info("Registered sums");

    g.transform("add_vect", add_vectors, concurrency::unlimited)
      .input_family("expanded_one", "expanded_three")
      .output_products("sums");

    g.transform("test_add_num", triple, concurrency::unlimited)
      .input_family("sums")
      .output_products("result");
    spdlog::info("Registered result");
  }

  SECTION("Duplicate product name and producer, differ only in type")
  {
    g.transform("square", square, concurrency::unlimited)
      .input_family("numbers")
      .output_products("square_result", "square_result");

    g.transform("extract_result", id, concurrency::unlimited)
      .input_family("square_result")
      .output_products("result");
  }

  g.observe("print_result", [](int res) { spdlog::info("Result: {}", res); })
    .input_family("result");
  spdlog::info("Registered observe");
  g.execute();
  spdlog::info("Executed");
}

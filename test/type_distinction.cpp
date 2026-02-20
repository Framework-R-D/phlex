#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "spdlog/spdlog.h"

#include "catch2/catch_test_macros.hpp"

#include <tuple>
#include <vector>

using namespace phlex;

namespace {
  // Provider functions
  int provide_numbers(data_cell_index const& index) { return static_cast<int>(index.number()); }

  std::size_t provide_length(data_cell_index const& index) { return index.number(); }

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
    auto job_index = data_cell_index::base_ptr();
    driver.yield(job_index);
    for (int i : numbers) {
      auto event_index = job_index->make_child(unsigned(i), "event");
      driver.yield(event_index);
    }
  };

  experimental::framework_graph g{gen};

  // Register providers
  g.provide("provide_numbers", provide_numbers, concurrency::unlimited)
    .output_product(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id});
  g.provide("provide_length", provide_length, concurrency::unlimited)
    .output_product(
      product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "length"_id});

  SECTION("Duplicate product name but differ in creator name")
  {
    g.observe("starter", [](int num) { spdlog::info("Received {}", num); })
      .input_family(
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id});
    g.transform("triple_numbers", triple, concurrency::unlimited)
      .input_family(
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id})
      .output_products("tripled");
    spdlog::info("Registered tripled");
    g.transform("expand_orig", expand, concurrency::unlimited)
      .input_family(
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id},
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "length"_id})
      .output_products("expanded_one");
    spdlog::info("Registered expanded_one");
    g.transform("expand_triples", expand, concurrency::unlimited)
      .input_family(
        product_query{.creator = "triple_numbers"_id, .layer = "event"_id, .suffix = "tripled"_id},
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "length"_id})
      .output_products("expanded_three");
    spdlog::info("Registered expanded_three");

    g.transform("add_nums", add_numbers, concurrency::unlimited)
      .input_family(
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id},
        product_query{.creator = "triple_numbers"_id, .layer = "event"_id, .suffix = "tripled"_id})
      .output_products("sums");
    spdlog::info("Registered sums");

    g.transform("add_vect", add_vectors, concurrency::unlimited)
      .input_family(
        product_query{
          .creator = "expand_orig"_id, .layer = "event"_id, .suffix = "expanded_one"_id},
        product_query{
          .creator = "expand_triples"_id, .layer = "event"_id, .suffix = "expanded_three"_id})
      .output_products("sums");

    g.transform("extract_result", triple, concurrency::unlimited)
      .input_family(
        product_query{.creator = "add_nums"_id, .layer = "event"_id, .suffix = "sums"_id})
      .output_products("result");
    spdlog::info("Registered result");
  }

  SECTION("Duplicate product name and creator, differ only in type")
  {
    g.transform("square", square, concurrency::unlimited)
      .input_family(
        product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "numbers"_id})
      .output_products("square_result", "square_result");

    g.transform("extract_result", id, concurrency::unlimited)
      .input_family(
        product_query{.creator = "square"_id, .layer = "event"_id, .suffix = "square_result"_id})
      .output_products("result");
  }

  g.observe("print_result", [](int res) { spdlog::info("Result: {}", res); })
    .input_family(
      product_query{.creator = "extract_result"_id, .layer = "event"_id, .suffix = "result"_id});
  spdlog::info("Registered observe");
  g.execute();
  spdlog::info("Executed");
}

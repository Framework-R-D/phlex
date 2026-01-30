#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_layer_hierarchy.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

using namespace phlex;

namespace {
  auto square_numbers(std::vector<unsigned> const& numbers)
  {
    std::vector<unsigned> result(size(numbers));
    std::transform(begin(numbers), end(numbers), begin(result), [](unsigned i) { return i * i; });
    return result;
  }

  auto sum_numbers(std::vector<unsigned> const& squared_numbers)
  {
    std::vector<unsigned> const expected_squared_numbers{0, 1, 4, 9, 16};
    CHECK(squared_numbers == expected_squared_numbers);
    return std::accumulate(begin(squared_numbers), end(squared_numbers), 0u);
  }

  double sqrt_sum_numbers(unsigned summed_numbers, unsigned offset)
  {
    CHECK(summed_numbers == 30u);
    return std::sqrt(static_cast<double>(summed_numbers + offset));
  }

  struct A {
    auto sqrt_sum(unsigned summed_numbers, unsigned offset) const
    {
      return sqrt_sum_numbers(summed_numbers, offset);
    }
  };
}

TEST_CASE("Call multiple functions", "[programming model]")
{
  experimental::framework_graph g{data_cell_index::base_ptr()};

  g.provide("provide_numbers",
            [](data_cell_index const&) -> std::vector<unsigned> { return {0, 1, 2, 3, 4}; })
    .output_product(
      product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "numbers"_id});
  g.provide("provide_offset", [](data_cell_index const&) -> unsigned { return 6u; })
    .output_product(product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "offset"_id});

  SECTION("All free functions")
  {
    g.transform("square_numbers", square_numbers, concurrency::unlimited)
      .input_family(product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "numbers"_id})
      .output_products("squared_numbers");
    g.transform("sum_numbers", sum_numbers, concurrency::unlimited)
      .input_family(product_query{
        .creator = "square_numbers"_id, .layer = "job"_id, .suffix = "squared_numbers"_id})
      .output_products("summed_numbers");
    g.transform("sqrt_sum", sqrt_sum_numbers, concurrency::unlimited)
      .input_family(product_query{.creator = "sum_numbers"_id,
                                  .layer = "job"_id,
                                  .suffix = "summed_numbers"_id},
                    product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "offset"_id})
      .output_products("result");
  }

  SECTION("Transforms, one from a class")
  {
    g.transform("square_numbers", square_numbers, concurrency::unlimited)
      .input_family(product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "numbers"_id})
      .output_products("squared_numbers");

    g.transform("sum_numbers", sum_numbers, concurrency::unlimited)
      .input_family(product_query{
        .creator = "square_numbers"_id, .layer = "job"_id, .suffix = "squared_numbers"_id})
      .output_products("summed_numbers");

    g.make<A>()
      .transform("sqrt_sum", &A::sqrt_sum, concurrency::unlimited)
      .input_family(product_query{.creator = "sum_numbers"_id,
                                  .layer = "job"_id,
                                  .suffix = "summed_numbers"_id},
                    product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "offset"_id})
      .output_products("result");
  }

  // The following is invoked for *each* section above
  g.observe("verify_result", [](double actual) { assert(actual == 6.); })
    .input_family(
      product_query{.creator = "sqrt_sum"_id, .layer = "job"_id, .suffix = "result"_id});
  g.execute();
}

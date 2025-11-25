#include "phlex/core/framework_graph.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

using namespace phlex::experimental;

namespace {
  auto square_numbers(std::vector<unsigned> const& numbers)
  {
    std::vector<unsigned> result(size(numbers));
    std::ranges::transform(numbers, begin(result), [](unsigned i) -> unsigned int { return i * i; });
    return result;
  }

  auto sum_numbers(std::vector<unsigned> const& squared_numbers)
  {
    std::vector<unsigned> const expected_squared_numbers{0, 1, 4, 9, 16};
    CHECK(squared_numbers == expected_squared_numbers);
    return std::accumulate(begin(squared_numbers), end(squared_numbers), 0u);
  }

  auto sqrt_sum_numbers(unsigned summed_numbers, unsigned offset) -> double
  {
    CHECK(summed_numbers == 30u);
    return std::sqrt(static_cast<double>(summed_numbers + offset));
  }

  struct A {
    [[nodiscard]] auto sqrt_sum(unsigned summed_numbers, unsigned offset) const
    {
      return sqrt_sum_numbers(summed_numbers, offset);
    }
  };
}

TEST_CASE("Call multiple functions", "[programming model]")
{
  auto store = product_store::base();
  store->add_product("numbers", std::vector<unsigned>{0, 1, 2, 3, 4});
  store->add_product("offset", 6u);
  framework_graph g{store};

  SECTION("All free functions")
  {
    g.transform("square_numbers", square_numbers, concurrency::unlimited)
      .input_family("numbers")
      .output_products("squared_numbers");
    g.transform("sum_numbers", sum_numbers, concurrency::unlimited)
      .input_family("squared_numbers")
      .output_products("summed_numbers");
    g.transform("sqrt_sum_numbers", sqrt_sum_numbers, concurrency::unlimited)
      .input_family("summed_numbers", "offset")
      .output_products("result");
  }

  SECTION("Transforms, one from a class")
  {
    g.transform("square_numbers", square_numbers, concurrency::unlimited)
      .input_family("numbers")
      .output_products("squared_numbers");

    g.transform("sum_numbers", sum_numbers, concurrency::unlimited)
      .input_family("squared_numbers")
      .output_products("summed_numbers");

    g.make<A>()
      .transform("sqrt_sum", &A::sqrt_sum, concurrency::unlimited)
      .input_family("summed_numbers", "offset")
      .output_products("result");
  }

  // The following is invoked for *each* section above
  g.observe("verify_result", [](double actual) -> void { assert(actual == 6.); }).input_family("result");
  g.execute();
}

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/level_hierarchy.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_all.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

using namespace phlex::experimental;

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
  auto store = product_store::base();
  store->add_product("numbers", std::vector<unsigned>{0, 1, 2, 3, 4});
  store->add_product("offset", 6u);
  framework_graph g{store};

  SECTION("All free functions")
  {
    g.with("square_numbers", square_numbers, concurrency::unlimited)
      .transform("numbers")
      .to("squared_numbers");
    g.with("sum_numbers", sum_numbers, concurrency::unlimited)
      .transform("squared_numbers")
      .to("summed_numbers");
    g.with("sqrt_sum_numbers", sqrt_sum_numbers, concurrency::unlimited)
      .transform("summed_numbers", "offset")
      .to("result");
  }

  SECTION("Transforms, one from a class")
  {
    g.with("square_numbers", square_numbers, concurrency::unlimited)
      .transform("numbers")
      .to("squared_numbers");
    g.with("sum_numbers", sum_numbers, concurrency::unlimited)
      .transform("squared_numbers")
      .to("summed_numbers");
    g.make<A>()
      .with("sqrt_sum", &A::sqrt_sum, concurrency::unlimited)
      .transform("summed_numbers", "offset")
      .to("result");
  }

  // The following is invoked for *each* section above
  g.with("verify_result", [](double actual) { assert(actual == 6.); }).observe("result");
  g.execute();
}

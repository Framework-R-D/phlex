#include "phlex/model/products.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

TEST_CASE("Reject null product pointers", "[data model]")
{
  using namespace phlex::detail;
  CHECK_THROWS_WITH(product_for(product_ptr{}), "Cannot store a null product pointer.");
  CHECK_THROWS_WITH(product_for(nullptr), "Cannot store a null product pointer.");
}

#include "phlex/core/product_query.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(""_in,
                    Catch::Matchers::ContainsSubstring("Cannot specify product with empty name."));
  CHECK_THROWS_WITH(
    "product"_in(""),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_query label{"product", {"event"}};
  CHECK(label == "product"_in("event"));
}

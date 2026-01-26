#include "phlex/core/product_query.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(
    product_query({.creator = ""s, .layer = "layer"s}),
    Catch::Matchers::ContainsSubstring("Cannot specify product with empty creator name."));
  CHECK_THROWS_WITH(
    product_query({.creator = "creator"s, .layer = ""s}),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_query label({.creator = "creator"s, .layer = "event"s, .suffix = "product"s});
  CHECK(label.creator() == "creator"s);
  CHECK(label.layer() == "event"s);
  CHECK(label.suffix() == "product"s);
}

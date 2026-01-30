#include "phlex/core/product_query.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(
    (product_query{.creator = ""_id, .layer = "layer"_id}),
    Catch::Matchers::ContainsSubstring("Cannot specify product with empty creator name."));
  CHECK_THROWS_WITH(
    (product_query{.creator = "creator"_id, .layer = ""_id}),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_query label({.creator = "creator"_id, .layer = "event"_id, .suffix = "product"_id});
  CHECK(label.creator == "creator"_id);
  CHECK(label.layer == "event"_id);
  CHECK(label.suffix == "product"_idq);
}

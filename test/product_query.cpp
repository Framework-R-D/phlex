#include "phlex/core/product_query.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(
    (product_query{.creator = "", .layer = "layer"}),
    Catch::Matchers::ContainsSubstring("Cannot specify product with empty creator name."));
  CHECK_THROWS_WITH(
    (product_query{.creator = "creator", .layer = ""}),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
  CHECK_THROWS_WITH(
    (product_query{.creator = "creator", .layer = "layer"}.spec()),
    Catch::Matchers::ContainsSubstring("Product suffixes are (temporarily) mandatory"));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_query label({.creator = "creator", .layer = "event", .suffix = "product"});
  CHECK(label.creator == "creator"_id);
  CHECK(label.layer == "event"_id);
  CHECK(label.suffix == "product"_idq);
  // Mismatched creator
  CHECK(!product_query{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_query{.creator = "2", .layer = "event", .suffix = "prod"}));
  // Mismatched layer
  CHECK(!product_query{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_query{.creator = "1", .layer = "event1", .suffix = "prod"}));
  // Mismatched suffix
  CHECK(!product_query{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_query{.creator = "1", .layer = "event", .suffix = "prod1"}));
  // Mismatched stage
  CHECK(
    !product_query{.creator = "1", .layer = "event", .suffix = "prod", .stage = "stage"_id}.match(
      product_query{.creator = "1", .layer = "event", .suffix = "prod", .stage = "stage1"_id}));
}

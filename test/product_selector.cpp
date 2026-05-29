#include "phlex/core/product_selector.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(
    (product_selector{.creator = "creator", .layer = ""}),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_selector label({.creator = "creator", .layer = "event", .suffix = "product"});
  CHECK(label.creator == "creator"_idq);
  CHECK(label.layer == "event"_idq);
  CHECK(label.suffix == "product"_id);
  // Mismatched creator
  CHECK(!product_selector{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "2", .layer = "event", .suffix = "prod"}));
  // Missing creator matches any creator
  CHECK(product_selector{.layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "2", .layer = "event", .suffix = "prod"}));
  // Mismatched layer
  CHECK(!product_selector{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "1", .layer = "event1", .suffix = "prod"}));
  // Mismatched suffix
  CHECK(!product_selector{.creator = "1", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "1", .layer = "event", .suffix = "prod1"}));
  // Mismatched stage
  CHECK(!product_selector{.creator = "1", .layer = "event", .suffix = "prod", .stage = "stage"_id}
           .match(product_selector{
             .creator = "1", .layer = "event", .suffix = "prod", .stage = "stage1"_id}));
  // Here for no reason other than to satisfy codecov
  CHECK(label == label);
  CHECK((label <=> label) == std::strong_ordering::equal);
}

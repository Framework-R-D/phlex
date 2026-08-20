#include "phlex/core/product_selector.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <string>

using namespace phlex;

TEST_CASE("Empty specifications", "[data model]")
{
  CHECK_THROWS_WITH(
    (product_selector{.creator = "", .layer = "layer"}),
    Catch::Matchers::ContainsSubstring("Cannot specify product with empty creator name."));
  CHECK_THROWS_WITH(
    (product_selector{.creator = "creator", .layer = ""}),
    Catch::Matchers::ContainsSubstring("Cannot specify the empty string as a data layer."));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_selector label({.creator = "creator", .layer = "event", .suffix = "product"});
  CHECK(*label.creator == "creator"_idq);
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

// Regression test for issue #707: the converting constructors of creator_name and
// required_layer_name once moved from their lvalue arguments, silently emptying them.
// Do not remove this test on the assumption that "specifying an lvalue as an argument
// can't possibly nullify it" -- that is exactly the bug this guards against.
TEST_CASE("Selectors do not move from lvalue arguments", "[data model]")
{
  experimental::identifier const expected_creator{"creator"};
  experimental::identifier creator{"creator"};
  experimental::identifier layer{"event"};
  std::string layer_string{"event"};

  product_selector first{.creator = creator, .layer = layer, .suffix = "a"_id};
  // The sources must be intact after the first use...
  CHECK(creator == expected_creator);
  CHECK(!layer.empty());
  // ...so that reusing them does not throw an empty-name error.
  product_selector second{.creator = creator, .layer = layer, .suffix = "b"_id};
  CHECK(first.creator == second.creator);
  CHECK(first.layer == second.layer);

  product_selector from_string{.creator = creator, .layer = layer_string};
  CHECK(layer_string == "event");
  CHECK(from_string.layer == second.layer);
}

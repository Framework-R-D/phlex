#include "phlex/core/product_selector.hpp"
#include "phlex/model/identifier.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fmt/format.h>

#include <string>

using namespace phlex;

TEST_CASE("Empty creator name", "[data model]")
{
  CHECK_THROWS_WITH(
    (product_selector{.creator = "", .layer = "layer"}),
    Catch::Matchers::ContainsSubstring("Cannot specify product with empty creator name."));
}

TEST_CASE("Creator name formatting", "[data model]")
{
  CHECK(fmt::format("{}", detail::creator_name{}) == "[ANY]");
  CHECK(fmt::format("{}", detail::creator_name{"creator"}) == "creator");
}

TEST_CASE("Empty layer name", "[data model]")
{
  CHECK_THROWS_WITH((product_selector{.creator = "creator", .layer = ""}),
                    Catch::Matchers::ContainsSubstring("Layer paths cannot be empty"));
}

TEST_CASE("Retrieving a non-existent layer name", "[data model]")
{
  product_selector const selector;
  CHECK_THROWS_WITH((static_cast<experimental::layer_path const&>(selector.layer)),
                    Catch::Matchers::ContainsSubstring(
                      "Cannot retrieve layer from product_selector with no layer"));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_selector label({.creator = "creator", .layer = "event", .suffix = "product"});
  CHECK(*label.creator == "creator"_idq);
  CHECK(label.layer->leaf() == "event"_idq);
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

TEST_CASE("Selecting by a multi-segment layer path", "[data model]")
{
  experimental::product_specification const spec{
    "creator", "prod"_id, experimental::make_type_id<int>()};
  experimental::identifier const stage{"stage"};
  experimental::layer_path const full{"/job/run/subrun/event"};

  SECTION("Selector with the leaf layer only")
  {
    CHECK(product_selector{.layer = "event"}.match(spec, full, stage));
  }
  SECTION("Selector with a trailing segment sequence")
  {
    CHECK(product_selector{.layer = "subrun/event"}.match(spec, full, stage));
    CHECK(product_selector{.layer = "run/subrun/event"}.match(spec, full, stage));
  }
  SECTION("Selector with the complete path")
  {
    CHECK(product_selector{.layer = "/job/run/subrun/event"}.match(spec, full, stage));
  }
  SECTION("Selector whose leaf matches but parent segment differs")
  {
    CHECK(!product_selector{.layer = "spill/event"}.match(spec, full, stage));
    CHECK(!product_selector{.layer = "run/event"}.match(spec, full, stage));
  }
  SECTION("Selector with a non-trailing segment only")
  {
    CHECK(!product_selector{.layer = "run/subrun"}.match(spec, full, stage));
  }
  SECTION("Selector longer than the layer path")
  {
    CHECK(!product_selector{.layer = "/job/extra/run/subrun/event"}.match(spec, full, stage));
  }
  SECTION("Complete selector path that differs from the layer path")
  {
    CHECK(!product_selector{.layer = "/job/subrun/event"}.match(spec, full, stage));
  }
  SECTION("Distinct layers sharing a leaf name")
  {
    experimental::layer_path const other{"/job/mc_sample/event"};
    product_selector const by_run{.layer = "run/event"};
    CHECK(!by_run.match(spec, other, stage));
    CHECK(product_selector{.layer = "mc_sample/event"}.match(spec, other, stage));
  }
  SECTION("Selector-to-selector matching")
  {
    product_selector const other{.layer = "/job/run/subrun/event"};
    CHECK(product_selector{.layer = "subrun/event"}.match(other));
    CHECK(!product_selector{.layer = "spill/event"}.match(other));
    CHECK(!product_selector{.layer = "run/subrun"}.match(other));
  }
}

#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fmt/format.h>

#include <string>

using namespace phlex;

TEST_CASE("Empty creator name", "[data model]")
{
  CHECK_THROWS_WITH((product_selector{.creator = "", .layer = "layer"}),
                    "Invalid identifier \"\": an identifier " +
                      std::string{phlex::detail::identifier_rule});
}

TEST_CASE("Creator name formatting", "[data model]")
{
  CHECK(fmt::format("{}", detail::creator_name{}) == "[ANY]");
  CHECK(fmt::format("{}", detail::creator_name{"creator"}) == "creator");
}

TEST_CASE("Empty layer name", "[data model]")
{
  CHECK_THROWS_WITH((product_selector{.creator = "creator", .layer = ""}),
                    "Invalid identifier \"\": an identifier " +
                      std::string{phlex::detail::identifier_rule});
}

TEST_CASE("Retrieving a non-existent layer name", "[data model]")
{
  product_selector const selector;
  CHECK_THROWS_WITH((static_cast<experimental::identifier const&>(selector.layer)),
                    Catch::Matchers::ContainsSubstring(
                      "Cannot retrieve layer from product_selector with no layer"));
}

TEST_CASE("Product name with data layer", "[data model]")
{
  product_selector label({.creator = "creator", .layer = "event", .suffix = "product"});
  CHECK(*label.creator == "creator"_idq);
  CHECK(*label.layer == "event"_idq);
  CHECK(label.suffix == "product"_id);
  // Mismatched creator
  CHECK(!product_selector{.creator = "one", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "two", .layer = "event", .suffix = "prod"}));
  // Missing creator matches any creator
  CHECK(product_selector{.layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "two", .layer = "event", .suffix = "prod"}));
  // Mismatched layer
  CHECK(!product_selector{.creator = "one", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "one", .layer = "event1", .suffix = "prod"}));
  // Mismatched suffix
  CHECK(!product_selector{.creator = "one", .layer = "event", .suffix = "prod"}.match(
    product_selector{.creator = "one", .layer = "event", .suffix = "prod1"}));
  // Mismatched stage
  CHECK(!product_selector{.creator = "one", .layer = "event", .suffix = "prod", .stage = "stage"_id}
           .match(product_selector{
             .creator = "one", .layer = "event", .suffix = "prod", .stage = "stage1"_id}));
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
  experimental::identifier const expected_layer{"event"};
  experimental::identifier creator{"creator"};
  experimental::identifier layer{"event"};
  std::string layer_string{"event"};

  product_selector first{.creator = creator, .layer = layer, .suffix = "a"_id};
  // The sources must be intact after the first use...
  CHECK(creator == expected_creator);
  CHECK(layer == expected_layer);
  // ...so that reusing them still yields the original values.
  product_selector second{.creator = creator, .layer = layer, .suffix = "b"_id};
  CHECK(first.creator == second.creator);
  CHECK(first.layer == second.layer);

  product_selector from_string{.creator = creator, .layer = layer_string};
  CHECK(layer_string == "event");
  CHECK(from_string.layer == second.layer);
}

TEST_CASE("Selectors match product specifications by creator, suffix, and type", "[data model]")
{
  using experimental::make_type_id;
  using experimental::product_specification;
  using experimental::suffix_t;
  using experimental::type_id;

  product_specification const qualified{
    experimental::algorithm_name::create("plugin:algo"), "prod"_id, make_type_id<int>()};
  product_specification const unqualified{
    experimental::algorithm_name::create("algo"), "prod"_id, make_type_id<int>()};
  product_specification const empty_suffix{
    experimental::algorithm_name::create("algo"), suffix_t{}, type_id{}};

  SECTION("A creator selects by plugin or by algorithm")
  {
    CHECK(product_selector{.creator = "plugin"}.match(qualified));
    CHECK(product_selector{.creator = "algo"}.match(qualified));
    CHECK(product_selector{.creator = "algo"}.match(unqualified));
    CHECK_FALSE(product_selector{.creator = "plugin"}.match(unqualified));
    CHECK_FALSE(product_selector{.creator = "other"}.match(qualified));
  }
  SECTION("An omitted creator or suffix is a wildcard")
  {
    CHECK(product_selector{}.match(qualified));
    CHECK(product_selector{}.match(empty_suffix));
    CHECK(product_selector{.creator = "algo"}.match(empty_suffix));
  }
  SECTION("An explicit empty suffix selects only the empty suffix")
  {
    CHECK(product_selector{.suffix = suffix_t{}}.match(empty_suffix));
    CHECK_FALSE(product_selector{.suffix = suffix_t{}}.match(unqualified));
    CHECK_FALSE(product_selector{.suffix = "prod"_id}.match(empty_suffix));
  }
  SECTION("A selector type must equal a specified specification type")
  {
    CHECK(product_selector{.type = make_type_id<int>()}.match(unqualified));
    CHECK_FALSE(product_selector{.type = make_type_id<double>()}.match(unqualified));
  }
}

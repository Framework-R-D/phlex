#include "phlex/core/resource/catalog.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <gsl/pointers>

#include <concepts>

using namespace phlex::detail;
using namespace phlex::test;

TEST_CASE("resource catalog", "[resource]")
{
  resource_catalog catalog;

  SECTION("registered resources can be looked up")
  {
    catalog.add_serialized<marker_value_token_resource>();
    CHECK_NOTHROW(catalog.limiter_for<marker_value_token_resource>());
  }

  SECTION("unlimited resources provide read-only access")
  {
    catalog.add_unlimited<integer_unlimited_resource>(42);
    auto const resource = catalog.access_for<integer_unlimited_resource>();
    STATIC_CHECK(
      std::same_as<decltype(resource), gsl::not_null<integer_unlimited_resource const*> const>);
    CHECK(resource->value == 42);
  }

  SECTION("serialized resources can provide a value")
  {
    catalog.add_serialized<value_token_resource>(42);
    CHECK_NOTHROW(catalog.limiter_for<value_token_resource>());
  }

  SECTION("duplicate serialized registrations throw")
  {
    catalog.add_serialized<marker_value_token_resource>();
    CHECK_THROWS_WITH(catalog.add_serialized<marker_value_token_resource>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has already been registered"));
  }

  SECTION("duplicate unlimited registrations throw")
  {
    catalog.add_unlimited<integer_unlimited_resource>(42);
    CHECK_THROWS_WITH(catalog.add_unlimited<integer_unlimited_resource>(43),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has already been registered"));
  }

  SECTION("missing resources throw")
  {
    CHECK_THROWS_WITH(catalog.limiter_for<marker_value_token_resource>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has not been registered"));
    CHECK_THROWS_WITH(catalog.access_for<integer_unlimited_resource>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has not been registered"));
  }
}

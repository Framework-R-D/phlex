#include "phlex/core/framework_graph.hpp"
#include "phlex/core/resource_api.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <gsl/pointers>

#include <array>
#include <concepts>

using namespace phlex;
using namespace phlex::detail;

namespace {
  struct catch2_resource {
    using token_type = catch2_resource;
  };

  struct unlimited_resource_type {
    explicit unlimited_resource_type(int v) : value{v} {}

    int value;
  };

  struct invalid_single_token_resource {
    using token_type = int*;
  };

  struct value_token_resource {
    using token_type = value_token_resource;

    explicit value_token_resource(int v) : value{v} {}

    int value;
  };

  struct pooled_token_resource {
    using token_type = int*;

    static auto tokens() { return std::array<int*, 1>{}; }
  };

  using mixed_dependencies = resource_dependencies<catch2_resource, unlimited_resource_type>;
  using reversed_dependencies = resource_dependencies<unlimited_resource_type, catch2_resource>;
  using bounded_dependencies = resource_dependencies<catch2_resource, catch2_resource>;

  // Unlimited resources.
  static_assert(unlimited_resource<unlimited_resource_type>);
  static_assert(not single_token_resource<unlimited_resource_type>);
  static_assert(std::same_as<internal::resource_access_type_t<unlimited_resource_type>,
                             unlimited_resource_type const*>);

  // Pointer-token single-token resources.
  static_assert(not unlimited_resource<catch2_resource>);
  static_assert(single_token_resource<catch2_resource>);
  static_assert(not pooled_resource<catch2_resource>);
  static_assert(std::same_as<internal::resource_access_type_t<catch2_resource>, catch2_resource>);
  static_assert(
    std::same_as<decltype(std::declval<resource_catalog const&>().limiter_for<catch2_resource>()),
                 tbb::flow::resource_limiter<catch2_resource>&>);

  // Value-token single-token resources.
  static_assert(single_token_resource<value_token_resource>);
  static_assert(
    std::same_as<internal::resource_access_type_t<value_token_resource>, value_token_resource>);
  static_assert(std::same_as<decltype(std::declval<resource_catalog const&>()
                                        .limiter_for<value_token_resource>()),
                             tbb::flow::resource_limiter<value_token_resource>&>);

  // Invalid token types.
  static_assert(not resource_object_token_type<invalid_single_token_resource>);
  static_assert(not single_token_resource<invalid_single_token_resource>);
  static_assert(not phase_1_resource<invalid_single_token_resource>);

  // Pooled resources, deferred in Phase 1.
  static_assert(pooled_resource<pooled_token_resource>);
  static_assert(not single_token_resource<pooled_token_resource>);
  static_assert(not phase_1_resource<pooled_token_resource>);

  // Resource dependency partitioning.
  static_assert(mixed_dependencies::has_bounded_resources);
  static_assert(
    std::same_as<mixed_dependencies::bounded_resources, boost::mp11::mp_list<catch2_resource>>);
  static_assert(std::same_as<mixed_dependencies::unlimited_resources,
                             boost::mp11::mp_list<unlimited_resource_type>>);
  static_assert(std::same_as<mixed_dependencies::bounded_resource_indices, std::index_sequence<0>>);
  static_assert(
    std::same_as<mixed_dependencies::unlimited_resource_indices, std::index_sequence<1>>);

  static_assert(reversed_dependencies::has_bounded_resources);
  static_assert(
    std::same_as<reversed_dependencies::bounded_resource_indices, std::index_sequence<1>>);
  static_assert(
    std::same_as<reversed_dependencies::unlimited_resource_indices, std::index_sequence<0>>);

  static_assert(bounded_dependencies::has_bounded_resources);
  static_assert(
    std::same_as<bounded_dependencies::bounded_resource_indices, std::index_sequence<0, 1>>);
  static_assert(
    std::same_as<bounded_dependencies::unlimited_resource_indices, std::index_sequence<>>);
  static_assert(not resource_dependencies<unlimited_resource_type>::has_bounded_resources);
  static_assert(not resource_dependencies<>::has_bounded_resources);
}

TEST_CASE("resource catalog", "[resource]")
{
  resource_catalog catalog;

  SECTION("registered resources can be looked up")
  {
    catalog.add<catch2_resource>();
    CHECK_NOTHROW(catalog.limiter_for<catch2_resource>());
  }

  SECTION("unlimited resources provide read-only access")
  {
    catalog.add<unlimited_resource_type>(42);
    auto const resource = catalog.access_for<unlimited_resource_type>();
    STATIC_CHECK(
      std::same_as<decltype(resource), gsl::not_null<unlimited_resource_type const*> const>);
    CHECK(resource->value == 42);
  }

  SECTION("single-token resources can provide a value")
  {
    catalog.add<value_token_resource>(42);
    CHECK_NOTHROW(catalog.limiter_for<value_token_resource>());
  }

  SECTION("duplicate registrations throw")
  {
    catalog.add<catch2_resource>();
    CHECK_THROWS_WITH(catalog.add<catch2_resource>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has already been registered"));
  }

  SECTION("missing resources throw")
  {
    CHECK_THROWS_WITH(catalog.limiter_for<catch2_resource>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has not been registered"));
    CHECK_THROWS_WITH(catalog.access_for<unlimited_resource_type>(),
                      Catch::Matchers::ContainsSubstring("Resource of type '") &&
                        Catch::Matchers::ContainsSubstring("' has not been registered"));
  }
}

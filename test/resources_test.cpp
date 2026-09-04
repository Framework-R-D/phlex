#include "phlex/core/framework_graph.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <gsl/pointers>

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>

using namespace phlex;
using namespace phlex::detail;
using namespace std::chrono_literals;

namespace {
  struct catch2_resource {
    using token_type = catch2_resource;
  };

  struct pointer_resource {
    using token_type = pointer_resource const*;
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

TEST_CASE("registering a pointer resource with the graph", "[graph][resource]")
{
  auto g = framework_graph::with_default_driver();
  g.add_resource<pointer_resource>();

  g.provide(
     "number_maker", [](data_cell_index const&) { return 42; }, concurrency::unlimited)
    .output_product("number_maker", "", "job");

  auto counter = thread_counter::counter_type{};
  // Catch2 assertions are not thread-safe, so each observer records its
  // observation and the expectations are checked after the graph completes.
  std::atomic<unsigned int> expected_numbers_seen{};
  auto verify_number = [&counter, &expected_numbers_seen](int const num, pointer_resource const*) {
    // Both observers share one resource token and must not run concurrently.
    thread_counter throw_if_more_than_one_thread{counter};
    if (num == 42) {
      ++expected_numbers_seen;
    }
    spin_for(1ms);
  };

  product_selector const number_selector{.creator = "number_maker", .layer = "job"};
  g.observe("verify1", verify_number, concurrency::unlimited)
    .input_family(number_selector, resource<pointer_resource>{});
  g.observe("verify2", verify_number, concurrency::unlimited)
    .input_family(number_selector, resource<pointer_resource>{});

  g.execute();

  CHECK(g.execution_count("verify1") == 1);
  CHECK(g.execution_count("verify2") == 1);
  // One observation per observer, each of the expected number.
  CHECK(expected_numbers_seen == 2u);
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

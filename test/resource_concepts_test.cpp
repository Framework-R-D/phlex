#include "phlex/core/framework_graph.hpp"
#include "phlex/core/resource/catalog.hpp"
#include "phlex/core/resource/concepts.hpp"
#include "test/resources_for_testing.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <array>
#include <concepts>
#include <utility>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;

namespace {
  struct invalid_serialized_resource {
    using token_type = int*;
  };

  struct const_member_token_resource {
    using token_type = int;

    // No state is necessary for this test, so we suppress the clang-tidy warning.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    int token() const { return 24; }
  };

  // A mutable-only tokens() member does not affect the unlimited classification.
  struct mutable_tokens_unlimited_resource {
    void tokens() {}
  };

  // A mutable-only tokens() member does not make a token-bearing resource bounded.
  struct mutable_tokens_serialized_resource {
    using token_type = mutable_tokens_serialized_resource const*;

    void tokens() {}
  };

  struct pooled_token_resource {
    using token_type = int*;

    static auto tokens() { return std::array<int*, 1>{}; }
  };

  template <typename T>
  concept graph_accepts_unlimited =
    requires(framework_graph& graph) { graph.template add_unlimited_resource<T>(); };

  template <typename T>
  concept graph_accepts_serialized =
    requires(framework_graph& graph) { graph.template add_serialized_resource<T>(); };

  template <typename T>
  concept catalog_accepts_unlimited =
    requires(resource_catalog& catalog) { catalog.template add_unlimited<T>(); };

  template <typename T>
  concept catalog_accepts_serialized =
    requires(resource_catalog& catalog) { catalog.template add_serialized<T>(); };

  static_assert(graph_accepts_unlimited<unlimited_test_resource>);
  static_assert(not graph_accepts_serialized<unlimited_test_resource>);
  static_assert(catalog_accepts_unlimited<unlimited_test_resource>);
  static_assert(not catalog_accepts_serialized<unlimited_test_resource>);

  static_assert(not graph_accepts_unlimited<pointer_token_resource>);
  static_assert(graph_accepts_serialized<pointer_token_resource>);
  static_assert(not catalog_accepts_unlimited<pointer_token_resource>);
  static_assert(catalog_accepts_serialized<pointer_token_resource>);

  static_assert(not graph_accepts_unlimited<custom_token_resource>);
  static_assert(graph_accepts_serialized<custom_token_resource>);
  static_assert(not catalog_accepts_unlimited<custom_token_resource>);
  static_assert(catalog_accepts_serialized<custom_token_resource>);

  static_assert(not graph_accepts_unlimited<const_member_token_resource>);
  static_assert(graph_accepts_serialized<const_member_token_resource>);
  static_assert(not catalog_accepts_unlimited<const_member_token_resource>);
  static_assert(catalog_accepts_serialized<const_member_token_resource>);

  static_assert(graph_accepts_unlimited<mutable_tokens_unlimited_resource>);
  static_assert(not graph_accepts_serialized<mutable_tokens_unlimited_resource>);
  static_assert(catalog_accepts_unlimited<mutable_tokens_unlimited_resource>);
  static_assert(not catalog_accepts_serialized<mutable_tokens_unlimited_resource>);

  static_assert(not graph_accepts_unlimited<mutable_tokens_serialized_resource>);
  static_assert(graph_accepts_serialized<mutable_tokens_serialized_resource>);
  static_assert(not catalog_accepts_unlimited<mutable_tokens_serialized_resource>);
  static_assert(catalog_accepts_serialized<mutable_tokens_serialized_resource>);

  static_assert(not graph_accepts_unlimited<pooled_token_resource>);
  static_assert(not graph_accepts_serialized<pooled_token_resource>);
  static_assert(not catalog_accepts_unlimited<pooled_token_resource>);
  static_assert(not catalog_accepts_serialized<pooled_token_resource>);

  static_assert(not graph_accepts_unlimited<invalid_serialized_resource>);
  static_assert(not graph_accepts_serialized<invalid_serialized_resource>);
  static_assert(not catalog_accepts_unlimited<invalid_serialized_resource>);
  static_assert(not catalog_accepts_serialized<invalid_serialized_resource>);

  // Unlimited resources.
  static_assert(unlimited_resource<integer_unlimited_resource>);
  static_assert(not serialized_resource<integer_unlimited_resource>);
  static_assert(std::same_as<internal::resource_access_type_t<integer_unlimited_resource>,
                             integer_unlimited_resource const*>);

  // Marker value-token serialized resources.
  static_assert(not unlimited_resource<marker_value_token_resource>);
  static_assert(serialized_resource<marker_value_token_resource>);
  static_assert(not pooled_resource<marker_value_token_resource>);
  static_assert(std::same_as<internal::resource_access_type_t<marker_value_token_resource>,
                             marker_value_token_resource>);
  static_assert(std::same_as<decltype(std::declval<resource_catalog const&>()
                                        .limiter_for<marker_value_token_resource>()),
                             tbb::flow::resource_limiter<marker_value_token_resource>&>);

  // Value-token serialized resources.
  static_assert(serialized_resource<value_token_resource>);
  static_assert(
    std::same_as<internal::resource_access_type_t<value_token_resource>, value_token_resource>);
  static_assert(std::same_as<decltype(std::declval<resource_catalog const&>()
                                        .limiter_for<value_token_resource>()),
                             tbb::flow::resource_limiter<value_token_resource>&>);

  // Custom-token serialized resources.
  static_assert(serialized_resource<custom_token_resource>);
  static_assert(std::same_as<decltype(std::declval<resource_catalog const&>()
                                        .limiter_for<custom_token_resource>()),
                             tbb::flow::resource_limiter<int>&>);

  // Invalid token types.
  static_assert(not resource_token_constructible<invalid_serialized_resource>);
  static_assert(not serialized_resource<invalid_serialized_resource>);
  static_assert(not phase_1_supported_resource<invalid_serialized_resource>);

  // Pooled resources, deferred in Phase 1.
  static_assert(pooled_resource<pooled_token_resource>);
  static_assert(not serialized_resource<pooled_token_resource>);
  static_assert(not phase_1_supported_resource<pooled_token_resource>);
}

int main() {}

#include "phlex/core/resource/dependencies.hpp"
#include "test/resources_for_testing.hpp"

// Public provider of mp_list; include-cleaner sees only Boost's ignored detail declaration.
#include <boost/mp11/list.hpp> // IWYU pragma: keep

#include <concepts>
#include <utility>

using namespace phlex::detail;
using namespace phlex::test;

namespace {
  using mixed_dependencies =
    resource_dependencies<marker_value_token_resource, integer_unlimited_resource>;
  using reversed_dependencies =
    resource_dependencies<integer_unlimited_resource, marker_value_token_resource>;
  using serialized_dependencies =
    resource_dependencies<marker_value_token_resource, marker_value_token_resource>;

  // Resource dependency partitioning.
  static_assert(mixed_dependencies::has_serialized_resources);
  static_assert(std::same_as<mixed_dependencies::serialized_resources,
                             boost::mp11::mp_list<marker_value_token_resource>>);
  static_assert(std::same_as<mixed_dependencies::unlimited_resources,
                             boost::mp11::mp_list<integer_unlimited_resource>>);
  static_assert(
    std::same_as<mixed_dependencies::serialized_resource_indices, std::index_sequence<0>>);
  static_assert(
    std::same_as<mixed_dependencies::unlimited_resource_indices, std::index_sequence<1>>);

  static_assert(reversed_dependencies::has_serialized_resources);
  static_assert(
    std::same_as<reversed_dependencies::serialized_resource_indices, std::index_sequence<1>>);
  static_assert(
    std::same_as<reversed_dependencies::unlimited_resource_indices, std::index_sequence<0>>);

  static_assert(serialized_dependencies::has_serialized_resources);
  static_assert(
    std::same_as<serialized_dependencies::serialized_resource_indices, std::index_sequence<0, 1>>);
  static_assert(
    std::same_as<serialized_dependencies::unlimited_resource_indices, std::index_sequence<>>);
  static_assert(not resource_dependencies<integer_unlimited_resource>::has_serialized_resources);
  static_assert(not resource_dependencies<>::has_serialized_resources);
}

int main() {}

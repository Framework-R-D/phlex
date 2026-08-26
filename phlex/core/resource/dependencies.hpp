#ifndef PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP
#define PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP

#include "phlex/core/resource/catalog.hpp"
#include "phlex/core/resource/index_sequences.hpp"

#include "boost/mp11/algorithm.hpp"
#include "boost/mp11/list.hpp"

#include <tuple>

namespace phlex::detail {
  // Retains the registration order while deriving the serialized and unlimited subsets needed
  // by node construction. The index sequences let the invocation layer restore the original
  // order after TBB has acquired only the serialized-resource tokens.
  template <typename... Resources>
  struct resource_dependencies {
    static_assert((phase_1_supported_resource<Resources> && ...),
                  "resource<T> dependencies currently support only unlimited or serialized "
                  "resources.");

    using all_resources = boost::mp11::mp_list<Resources...>;
    using serialized_resources =
      boost::mp11::mp_copy_if<all_resources, internal::is_serialized_resource>;
    using unlimited_resources =
      boost::mp11::mp_copy_if<all_resources, internal::is_unlimited_resource>;
    using serialized_resource_indices =
      internal::serialized_resource_indices<0, Resources...>::type;
    using unlimited_resource_indices = internal::unlimited_resource_indices<0, Resources...>::type;

    static constexpr bool has_serialized_resources =
      boost::mp11::mp_size<serialized_resources>::value != 0;

    // Unlimited resources are cached once per node (access pointers are stable); serialized
    // resources are looked up as the TBB resource_limiter references resource_limited_node needs.
    static auto unlimited_resource_accesses(resource_catalog const& resources)
    {
      return [&resources]<typename... Unlimited>(boost::mp11::mp_list<Unlimited...>) {
        // The cached tuple decays the catalog's gsl::not_null values to algorithm-facing pointers.
        return std::tuple<internal::resource_access_type_t<Unlimited>...>{
          resources.template access_for<Unlimited>()...};
      }(unlimited_resources{});
    }

    static auto serialized_resource_limiters(resource_catalog& resources)
    {
      return [&resources]<typename... Serialized>(boost::mp11::mp_list<Serialized...>) {
        return std::tie(resources.template limiter_for<Serialized>()...);
      }(serialized_resources{});
    }
  };

  template <typename... Resources>
  inline constexpr bool has_serialized =
    resource_dependencies<Resources...>::has_serialized_resources;
}

#endif // PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP

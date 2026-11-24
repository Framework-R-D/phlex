#ifndef PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP
#define PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP

#include "phlex/core/resource/catalog.hpp"
#include "phlex/core/resource/index_sequences.hpp"

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <tuple>

namespace phlex::detail {
  // Retains registration order while deriving the limited and unlimited subsets needed by node
  // construction. The index sequences restore the original order after TBB acquires limited tokens.
  template <typename... Resources>
  struct resource_dependencies {
    static_assert(((unlimited_resource<Resources> || limited_resource<Resources>) && ...),
                  "resource<T> dependencies must be unlimited, serialized, or pooled resources.");

    using all_resources = boost::mp11::mp_list<Resources...>;
    using limited_resources = boost::mp11::mp_copy_if<all_resources, internal::is_limited_resource>;
    using unlimited_resources =
      boost::mp11::mp_copy_if<all_resources, internal::is_unlimited_resource>;
    using limited_resource_indices =
      internal::matching_indices<internal::is_limited_resource, Resources...>;
    using unlimited_resource_indices =
      internal::matching_indices<internal::is_unlimited_resource, Resources...>;

    static constexpr bool has_limited_resources = limited_resource_indices::size() != 0;

    // Unlimited resources are cached once per node (access pointers are stable); serialized
    // resources are looked up as the TBB resource_limiter references limited_resource_node needs.
    static auto unlimited_resource_accesses(resource_catalog const& resources)
    {
      return [&resources]<typename... Unlimited>(boost::mp11::mp_list<Unlimited...>) {
        // The cached tuple decays the catalog's gsl::not_null values to algorithm-facing pointers.
        return std::tuple<internal::resource_access_type_t<Unlimited>...>{
          resources.template access_for<Unlimited>()...};
      }(unlimited_resources{});
    }

    static auto limited_resource_limiters(resource_catalog& resources)
    {
      return [&resources]<typename... Limited>(boost::mp11::mp_list<Limited...>) {
        return std::tie(resources.template limiter_for<Limited>()...);
      }(limited_resources{});
    }
  };

  template <typename... Resources>
  inline constexpr bool has_limited = resource_dependencies<Resources...>::has_limited_resources;
}

#endif // PHLEX_CORE_RESOURCE_DEPENDENCIES_HPP

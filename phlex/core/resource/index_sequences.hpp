#ifndef PHLEX_CORE_RESOURCE_INDEX_SEQUENCES_HPP
#define PHLEX_CORE_RESOURCE_INDEX_SEQUENCES_HPP

#include "phlex/core/resource/concepts.hpp"

#include <cstddef>
#include <utility>

namespace phlex::detail::internal {
  template <typename First, typename Second>
  struct concatenate_index_sequences;

  template <std::size_t... First, std::size_t... Second>
  struct concatenate_index_sequences<std::index_sequence<First...>,
                                     std::index_sequence<Second...>> {
    using type = std::index_sequence<First..., Second...>;
  };

  template <std::size_t I, typename... Resources>
  struct bounded_resource_indices;

  template <std::size_t I>
  struct bounded_resource_indices<I> {
    using type = std::index_sequence<>;
  };

  template <std::size_t I, typename Resource, typename... Resources>
  struct bounded_resource_indices<I, Resource, Resources...> {
    using tail = bounded_resource_indices<I + 1, Resources...>::type;
    using type =
      std::conditional_t<single_token_resource<Resource>,
                         typename concatenate_index_sequences<std::index_sequence<I>, tail>::type,
                         tail>;
  };

  template <std::size_t I, typename... Resources>
  struct unlimited_resource_indices;

  template <std::size_t I>
  struct unlimited_resource_indices<I> {
    using type = std::index_sequence<>;
  };

  template <std::size_t I, typename Resource, typename... Resources>
  struct unlimited_resource_indices<I, Resource, Resources...> {
    using tail = unlimited_resource_indices<I + 1, Resources...>::type;
    using type =
      std::conditional_t<unlimited_resource<Resource>,
                         typename concatenate_index_sequences<std::index_sequence<I>, tail>::type,
                         tail>;
  };
}

#endif // PHLEX_CORE_RESOURCE_INDEX_SEQUENCES_HPP

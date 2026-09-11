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

  template <std::size_t I, template <typename> typename Predicate, typename... Resources>
  struct matching_indices_impl;

  template <std::size_t I, template <typename> typename Predicate>
  struct matching_indices_impl<I, Predicate> {
    using type = std::index_sequence<>;
  };

  template <std::size_t I,
            template <typename> typename Predicate,
            typename Resource,
            typename... Resources>
  struct matching_indices_impl<I, Predicate, Resource, Resources...> {
    using tail = matching_indices_impl<I + 1, Predicate, Resources...>::type;
    using type =
      std::conditional_t<Predicate<Resource>::value,
                         typename concatenate_index_sequences<std::index_sequence<I>, tail>::type,
                         tail>;
  };

  template <template <typename> typename Predicate, typename... Resources>
  using matching_indices = matching_indices_impl<0, Predicate, Resources...>::type;
}

#endif // PHLEX_CORE_RESOURCE_INDEX_SEQUENCES_HPP

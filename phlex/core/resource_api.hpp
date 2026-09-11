#ifndef PHLEX_CORE_RESOURCE_API_HPP
#define PHLEX_CORE_RESOURCE_API_HPP

#include "phlex/core/resource/dependencies.hpp"

#include "boost/mp11/algorithm.hpp"
#include "boost/mp11/list.hpp"
#include "boost/mp11/set.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace phlex {
  template <typename T>
  struct resource {
    static_assert(!std::is_const_v<T>, "resource<T> must name a non-const resource type.");
  };
}

namespace phlex::detail {
  namespace internal {
    template <typename T>
    struct is_resource_impl : std::false_type {};

    template <typename T>
    struct is_resource_impl<resource<T>> : std::true_type {};

    template <typename T>
    using is_resource_p = is_resource_impl<std::remove_cvref_t<T>>;
  }

  template <typename T>
  concept is_resource = internal::is_resource_impl<std::remove_cvref_t<T>>::value;

  namespace internal {
    template <typename T>
    struct resource_type;

    template <typename T>
    struct resource_type<resource<T>> {
      using type = T;
    };

    template <typename T>
    using resource_type_t = resource_type<std::remove_cvref_t<T>>::type;
  }

  // Finds the first trailing resource<T> argument in an input_family(...) argument list.
  template <typename... Args>
  struct resource_split {
    using arguments = boost::mp11::mp_list<std::remove_cvref_t<Args>...>;

    static constexpr std::size_t index =
      boost::mp11::mp_find_if<arguments, internal::is_resource_p>::value;

    using selector_types = boost::mp11::mp_take_c<arguments, index>;
    using resource_types = boost::mp11::mp_drop_c<arguments, index>;
    using resources_type = boost::mp11::mp_rename<resource_types, std::tuple>;

    static_assert(boost::mp11::mp_is_set<resource_types>::value,
                  "Each resource type may only be specified once.");

    static constexpr std::size_t number_selectors = index;
    static constexpr std::size_t number_resources = boost::mp11::mp_size<resource_types>::value;
    static constexpr bool resources_are_last =
      boost::mp11::mp_all_of<resource_types, internal::is_resource_p>::value;
  };
}

#endif // PHLEX_CORE_RESOURCE_API_HPP

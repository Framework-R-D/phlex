#ifndef PHLEX_CORE_RESOURCE_CONCEPTS_HPP
#define PHLEX_CORE_RESOURCE_CONCEPTS_HPP

#include <concepts>
#include <type_traits>

namespace phlex::detail {
  namespace internal {
    template <typename T>
    concept has_token_type = requires { typename T::token_type; };

    template <typename T>
    concept has_tokens = requires(T& resource) { resource.tokens(); };
  }

  template <typename T>
  concept unlimited_resource = !internal::has_token_type<T>;

  template <typename T>
  concept resource_object_token_type =
    internal::has_token_type<T> &&
    (std::convertible_to<T*, typename T::token_type> ||
     (std::same_as<typename T::token_type, T> && std::copy_constructible<T>));

  template <typename T>
  concept single_token_resource = resource_object_token_type<T> && !internal::has_tokens<T>;

  template <typename T>
  concept pooled_resource = internal::has_token_type<T> && internal::has_tokens<T>;

  template <typename T>
  concept phase_1_resource = unlimited_resource<T> || single_token_resource<T>;

  namespace internal {
    template <typename T, bool = single_token_resource<T>>
    struct resource_access_type {
      using type = T const*;
    };

    template <typename T>
    struct resource_access_type<T, true> {
      using type = T::token_type;
    };

    template <typename T>
    using resource_access_type_t = resource_access_type<T>::type;

    template <typename T>
    using is_unlimited_resource = std::bool_constant<unlimited_resource<T>>;

    template <typename T>
    using is_single_token_resource = std::bool_constant<single_token_resource<T>>;
  }
}

#endif // PHLEX_CORE_RESOURCE_CONCEPTS_HPP

#ifndef PHLEX_CORE_RESOURCE_CONCEPTS_HPP
#define PHLEX_CORE_RESOURCE_CONCEPTS_HPP

#include <concepts>
#include <type_traits>

namespace phlex::detail {
  namespace internal {
    template <typename T>
    concept has_token_type = requires { typename T::token_type; };

    template <typename T>
    concept has_tokens = requires(T const& resource) { resource.tokens(); };
  }

  template <typename T>
  concept unlimited_resource = !internal::has_token_type<T>;

  template <typename T>
  concept resource_token_constructible =
    internal::has_token_type<T> &&
    ((std::same_as<typename T::token_type, T> && std::copy_constructible<T>) ||
     std::convertible_to<T*, typename T::token_type> || requires(T const& resource) {
       { resource.token() } -> std::convertible_to<typename T::token_type>;
     });

  template <typename T>
  concept serialized_resource = resource_token_constructible<T> && !internal::has_tokens<T>;

  template <typename T>
  concept pooled_resource = internal::has_token_type<T> && internal::has_tokens<T>;

  template <typename T>
  concept phase_1_supported_resource = unlimited_resource<T> || serialized_resource<T>;

  template <typename T, typename... Args>
  concept unlimited_resource_registration =
    unlimited_resource<T> && !std::is_const_v<T> && std::constructible_from<T, Args...>;

  template <typename T, typename... Args>
  concept serialized_resource_registration =
    serialized_resource<T> && !std::is_const_v<T> && std::constructible_from<T, Args...>;

  namespace internal {
    template <typename T, bool = serialized_resource<T>>
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
    using is_serialized_resource = std::bool_constant<serialized_resource<T>>;
  }
}

#endif // PHLEX_CORE_RESOURCE_CONCEPTS_HPP

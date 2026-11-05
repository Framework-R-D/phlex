#ifndef PHLEX_CORE_RESOURCE_CONCEPTS_HPP
#define PHLEX_CORE_RESOURCE_CONCEPTS_HPP

#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>

namespace phlex::detail {
  namespace internal {
    template <typename T>
    concept has_token_type = requires { typename T::token_type; };

    template <typename T>
    concept has_tokens = requires(T& resource) { resource.tokens(); };

    template <typename T>
    using pooled_token_range_t = decltype(std::declval<T const&>().tokens());

    template <typename T>
    using pooled_token_type_t = std::ranges::range_value_t<pooled_token_range_t<T>>;

    template <typename T>
    concept has_valid_token_range = requires(T const& resource) {
      { resource.tokens() } -> std::ranges::input_range;
      requires std::copy_constructible<pooled_token_type_t<T>>;
      requires std::is_move_assignable_v<pooled_token_type_t<T>>;
      requires std::constructible_from<pooled_token_type_t<T>,
                                       std::ranges::range_reference_t<pooled_token_range_t<T>>>;
    };
  }

  template <typename T>
  concept unlimited_resource = !internal::has_token_type<T> && !internal::has_tokens<T>;

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
  concept pooled_resource = internal::has_valid_token_range<T>;

  template <typename T>
  concept limited_resource = serialized_resource<T> || pooled_resource<T>;

  template <typename T, typename... Args>
  concept unlimited_resource_registration =
    unlimited_resource<T> && !std::is_const_v<T> && std::constructible_from<T, Args...>;

  template <typename T, typename... Args>
  concept serialized_resource_registration =
    serialized_resource<T> && !std::is_const_v<T> && std::constructible_from<T, Args...>;

  template <typename T, typename... Args>
  concept pooled_resource_registration =
    pooled_resource<T> && !std::is_const_v<T> && std::constructible_from<T, Args...>;

  namespace internal {
    template <typename T>
    struct resource_access_type;

    template <unlimited_resource T>
    struct resource_access_type<T> {
      using type = T const*;
    };

    template <serialized_resource T>
    struct resource_access_type<T> {
      using type = T::token_type;
    };

    template <pooled_resource T>
    struct resource_access_type<T> {
      using type = pooled_token_type_t<T>;
    };

    template <typename T>
    using resource_access_type_t = resource_access_type<T>::type;

    template <typename T>
    using is_unlimited_resource = std::bool_constant<unlimited_resource<T>>;

    template <typename T>
    using is_limited_resource = std::bool_constant<limited_resource<T>>;
  }
}

#endif // PHLEX_CORE_RESOURCE_CONCEPTS_HPP

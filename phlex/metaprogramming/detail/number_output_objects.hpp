#ifndef PHLEX_METAPROGRAMMING_DETAIL_NUMBER_OUTPUT_OBJECTS_HPP
#define PHLEX_METAPROGRAMMING_DETAIL_NUMBER_OUTPUT_OBJECTS_HPP

#include "phlex/metaprogramming/detail/basic_concepts.hpp"

#include <cstddef>
#include <tuple>

namespace phlex::experimental::detail {
  template <typename T>
  constexpr std::size_t number_types = 1ull;

  template <typename... Args>
  constexpr std::size_t number_types<std::tuple<Args...>> = sizeof...(Args);

  template <typename R>
  constexpr std::size_t number_types_not_void()
  {
    if constexpr (std::is_same<R, void>{}) {
      return 0ull;
    } else {
      return number_types<R>;
    }
  }

  // ============================================================================
  // Primary template is assumed to be a lambda
  template <typename T>
  constexpr std::size_t number_output_objects_impl =
    number_output_objects_impl<decltype(&T::operator())>;

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (T::*)(Args...)> = number_types_not_void<R>();

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (T::*)(Args...) const> =
    number_types_not_void<R>();

  template <typename R, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (*)(Args...)> = number_types_not_void<R>();

  template <typename R, typename... Args>
  constexpr std::size_t number_output_objects_impl<R(Args...)> = number_types_not_void<R>();

  // noexcept specializations
  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (T::*)(Args...) noexcept> =
    number_types_not_void<R>();

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (T::*)(Args...) const noexcept> =
    number_types_not_void<R>();

  template <typename R, typename... Args>
  constexpr std::size_t number_output_objects_impl<R (*)(Args...) noexcept> =
    number_types_not_void<R>();

  template <typename R, typename... Args>
  constexpr std::size_t number_output_objects_impl<R(Args...) noexcept> =
    number_types_not_void<R>();
}

#endif // PHLEX_METAPROGRAMMING_DETAIL_NUMBER_OUTPUT_OBJECTS_HPP

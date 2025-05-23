#ifndef phlex_metaprogramming_detail_number_parameters_hpp
#define phlex_metaprogramming_detail_number_parameters_hpp

#include "phlex/metaprogramming/detail/basic_concepts.hpp"

#include <cstddef>

namespace phlex::experimental::detail {
  // ============================================================================
  // Primary template is assumed to be a lambda
  template <typename T>
  constexpr std::size_t number_parameters_impl = number_parameters_impl<decltype(&T::operator())>;

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_parameters_impl<R (T::*)(Args...)> = sizeof...(Args);

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_parameters_impl<R (T::*)(Args...) const> = sizeof...(Args);

  template <typename R, typename... Args>
  constexpr std::size_t number_parameters_impl<R (*)(Args...)> = sizeof...(Args);

  template <typename R, typename... Args>
  constexpr std::size_t number_parameters_impl<R(Args...)> = sizeof...(Args);

  // noexcept specializations
  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_parameters_impl<R (T::*)(Args...) noexcept> = sizeof...(Args);

  template <typename R, typename T, typename... Args>
  constexpr std::size_t number_parameters_impl<R (T::*)(Args...) const noexcept> = sizeof...(Args);

  template <typename R, typename... Args>
  constexpr std::size_t number_parameters_impl<R (*)(Args...) noexcept> = sizeof...(Args);

  template <typename R, typename... Args>
  constexpr std::size_t number_parameters_impl<R(Args...) noexcept> = sizeof...(Args);
}

#endif // phlex_metaprogramming_detail_number_parameters_hpp

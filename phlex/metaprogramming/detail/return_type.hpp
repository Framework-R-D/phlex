#ifndef PHLEX_METAPROGRAMMING_DETAIL_RETURN_TYPE_HPP
#define PHLEX_METAPROGRAMMING_DETAIL_RETURN_TYPE_HPP

#include "phlex/metaprogramming/detail/basic_concepts.hpp"

namespace phlex::experimental::detail {
  // ============================================================================
  template <typename T, typename R, typename... Args>
  R return_type_impl(R (T::*)(Args...));

  template <typename T, typename R, typename... Args>
  R return_type_impl(R (T::*)(Args...) const);

  template <typename R, typename... Args>
  R return_type_impl(R (*)(Args...));

  // noexcept overlaods
  template <typename T, typename R, typename... Args>
  R return_type_impl(R (T::*)(Args...) noexcept);

  template <typename T, typename R, typename... Args>
  R return_type_impl(R (T::*)(Args...) const noexcept);

  template <typename R, typename... Args>
  R return_type_impl(R (*)(Args...) noexcept);

  template <has_call_operator T>
  auto return_type_impl(T&&) -> decltype(return_type_impl(&T::operator()));
}

#endif // PHLEX_METAPROGRAMMING_DETAIL_RETURN_TYPE_HPP

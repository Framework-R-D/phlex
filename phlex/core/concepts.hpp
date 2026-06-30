#ifndef PHLEX_CORE_CONCEPTS_HPP
#define PHLEX_CORE_CONCEPTS_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/fwd.hpp"

#include <concepts>
#include <utility>

namespace phlex::experimental {

  template <typename T>
  concept not_void = !std::same_as<T, void>;

  template <typename T>
  concept is_bound_object = not std::same_as<T, phlex::detail::void_tag>;

  template <typename T>
  concept sendable = std::move_constructible<T> || requires(T& t) {
    { send(t) } -> std::move_constructible;
  };

  template <typename T, std::size_t N>
  concept at_least_n_input_parameters = phlex::detail::number_parameters<T> >= N;

  template <typename T>
  concept at_least_one_input_parameter = at_least_n_input_parameters<T, 1>;

  template <typename T>
  concept at_least_two_input_parameters = at_least_n_input_parameters<T, 2>;

  template <typename T>
  concept at_least_one_output_object = phlex::detail::number_output_objects<T> >= 1ull;

  template <typename T>
  concept first_input_parameter_is_non_const_lvalue_reference =
    at_least_one_input_parameter<T> && phlex::detail::is_non_const_lvalue_reference<
                                         phlex::detail::function_parameter_type<0, T>>::value;

  template <typename T>
  concept first_input_parameter_is_sendable =
    at_least_one_input_parameter<T> && sendable<phlex::detail::function_parameter_type<0, T>>;

  template <typename T, typename R>
  concept returns = std::same_as<phlex::detail::return_type<T>, R>;

  template <typename T, typename... Args>
  concept expects_input_parameters = at_least_n_input_parameters<T, sizeof...(Args)> &&
                                     phlex::detail::check_parameters<T, Args...>::value;

  template <typename T>
  concept is_predicate_like = at_least_one_input_parameter<T> && returns<T, bool>;

  template <typename T>
  concept is_observer_like = at_least_one_input_parameter<T> && returns<T, void>;

  template <typename T>
  concept is_output_like = std::is_member_function_pointer_v<T> &&
                           expects_input_parameters<T, product_store const&> && returns<T, void>;

  template <typename T>
  concept is_provider_like = expects_input_parameters<T, data_cell_index const&> &&
                             phlex::detail::number_output_objects<T> == 1ull;

  template <typename T>
  concept is_fold_like =
    at_least_two_input_parameters<T> && first_input_parameter_is_non_const_lvalue_reference<T> &&
    first_input_parameter_is_sendable<T> &&
    returns<T, void>; // <= May change if data products can be created per fold step.

  template <typename T>
  concept is_unfold_like = expects_input_parameters<T, generator&> && returns<T, void>;

  template <typename T>
  concept is_transform_like = at_least_one_input_parameter<T> && at_least_one_output_object<T>;
}

#endif // PHLEX_CORE_CONCEPTS_HPP

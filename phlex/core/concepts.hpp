#ifndef PHLEX_CORE_CONCEPTS_HPP
#define PHLEX_CORE_CONCEPTS_HPP

#include "phlex/core/fold/send.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/metaprogramming/make_fold_like.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/fwd.hpp"

#include <concepts>
#include <utility>

namespace phlex::detail {

  template <typename T>
  concept not_void = !std::same_as<T, void>;

  template <typename T>
  concept is_bound_object = not std::same_as<T, void_tag>;

  template <typename T>
  concept sendable = requires { typename phlex::experimental::sendable_type<T>; };

  /// For use with Boost.Mp11 algorithms, which cannot take a concept.
  template <typename T>
  using is_sendable = std::bool_constant<sendable<T>>;

  template <typename T, std::size_t N>
  concept at_least_n_input_parameters = number_parameters<T> >= N;

  template <typename T>
  concept at_least_one_input_parameter = at_least_n_input_parameters<T, 1>;

  template <typename T>
  concept at_least_one_output_object = number_output_objects<T> >= 1ull;

  template <typename T>
  concept has_fold_accumulator = fold_like_traits<T>::num_accumulators::value >= 1;

  template <typename T>
  concept fold_accumulators_are_sendable =
    boost::mp11::mp_all_of<typename fold_like_traits<T>::accumulator_tuple_t, is_sendable>::value;

  template <typename T, typename R>
  concept returns = std::same_as<return_type<T>, R>;

  template <typename T, typename... Args>
  concept expects_input_parameters =
    at_least_n_input_parameters<T, sizeof...(Args)> && check_parameters<T, Args...>::value;

  template <typename T>
  concept is_predicate_like = at_least_one_input_parameter<T> && returns<T, bool>;

  template <typename T>
  concept is_observer_like = at_least_one_input_parameter<T> && returns<T, void>;

  template <typename T>
  concept is_output_like =
    expects_input_parameters<T, phlex::experimental::product_store const&> && returns<T, void>;

  template <typename T>
  concept is_provider_like =
    expects_input_parameters<T, data_cell_index const&> && number_output_objects<T> == 1ull;

  // fold_like_traits static_asserts that there is at least one input parameter.
  template <typename T>
  concept is_fold_like =
    has_fold_accumulator<T> && fold_accumulators_are_sendable<T> &&
    returns<T, void>; // <<< May change if data products can be created per fold step.

  template <typename T>
  concept is_unfold_like = expects_input_parameters<T, generator&> && returns<T, void>;

  template <typename T>
  concept is_transform_like = at_least_one_input_parameter<T> && at_least_one_output_object<T>;
}

#endif // PHLEX_CORE_CONCEPTS_HPP

#ifndef PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP
#define PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP

#include "phlex/metaprogramming/detail/ctor_reflect_types.hpp"

#include "boost/callable_traits.hpp"
#include "boost/mp11/algorithm.hpp"
#include "boost/mp11/list.hpp"

#include <atomic>
#include <iterator>
#include <tuple>
#include <type_traits>

namespace phlex::experimental {
  namespace ct = boost::callable_traits;
  namespace mp11 = boost::mp11;
  template <typename T>
  using return_type = ct::return_type_t<T>;

  template <typename T>
  using function_parameter_types =
    mp11::mp_eval_if_not<std::is_member_pointer<T>, ct::args_t<T>, mp11::mp_rest, ct::args_t<T>>;

  template <std::size_t I, typename T>
  using function_parameter_type = std::tuple_element_t<I, function_parameter_types<T>>;

  template <typename T>
  using constructor_parameter_types = typename refl::as_tuple<T>;

  template <typename T>
  constexpr std::size_t number_parameters = mp11::mp_size<function_parameter_types<T>>::value;

  // Wrapping in a tuple then "flattening" (which just removes one nested inner layer of tuple)
  // ensures a single T by itself becomes std::tuple<T> without changing anything that's already
  // a tuple.
  template <typename T>
  constexpr std::size_t number_types = mp11::mp_size<mp11::mp_flatten<std::tuple<T>>>::value;

  // Like the above on return_type<T> except we drop voids after wrapping in a tuple.
  // This means a plain void return type will return 0, not 1
  template <typename T>
  constexpr std::size_t number_output_objects = mp11::mp_size<
    mp11::mp_flatten<mp11::mp_remove_if<std::tuple<return_type<T>>, std::is_void>>>::value;

  template <typename Tuple>
  using skip_first_type = mp11::mp_rest<mp11::mp_apply<std::tuple, Tuple>>;

  template <typename T, typename... Args>
  struct check_parameters {
    using input_parameters = function_parameter_types<T>;
    static_assert(std::tuple_size<input_parameters>{} >= sizeof...(Args));
    static constexpr bool value =
      mp11::mp_starts_with<input_parameters, std::tuple<Args...>>::value;
    constexpr operator bool() noexcept { return value; }
  };

  // ===================================================================
  template <typename T>
  struct is_non_const_lvalue_reference : std::is_lvalue_reference<T> {};

  template <typename T>
  struct is_non_const_lvalue_reference<T const&> : std::false_type {};

  // mp_similar<std::atomic<int>, T> just checks if T is an atomic (of any type)
  template <typename T>
  using remove_atomic_t =
    mp11::mp_eval_if_not<mp11::mp_similar<std::atomic<int>, T>, T, mp11::mp_front, T>;

  template <typename T>
  concept container = requires {
    // NB: Just a few basics, not the full set of requirements of the Container named requirement
    typename T::iterator;
    typename T::value_type;
  };

  template <typename T>
  concept contiguous_container = requires {
    requires container<T>;
    requires std::contiguous_iterator<typename T::iterator>;
  };

  template <typename T>
  class is_tuple : public std::false_type {};

  template <typename... Ts>
  class is_tuple<std::tuple<Ts...>> : public std::true_type {};
}

#endif // PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP

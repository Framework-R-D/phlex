#ifndef PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP
#define PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP

#include "phlex/metaprogramming/detail/ctor_reflect_types.hpp"
#include "phlex/metaprogramming/detail/number_output_objects.hpp"
#include "phlex/metaprogramming/detail/number_parameters.hpp"
#include "phlex/metaprogramming/detail/parameter_types.hpp"
#include "phlex/metaprogramming/detail/return_type.hpp"

#include <atomic>
#include <iterator>
#include <tuple>
#include <type_traits>

namespace phlex::experimental {
  template <typename T>
  using return_type = decltype(detail::return_type_impl(std::declval<T>()));

  template <typename T>
  using function_parameter_types = decltype(detail::parameter_types_impl(std::declval<T>()));

  template <std::size_t I, typename T>
  using function_parameter_type = std::tuple_element_t<I, function_parameter_types<T>>;

  template <typename T>
  using constructor_parameter_types = typename refl::as_tuple<T>;

  template <typename T>
  constexpr std::size_t number_parameters = detail::number_parameters_impl<T>;

  template <typename T>
  constexpr std::size_t number_output_objects = detail::number_output_objects_impl<T>;

  using detail::number_types;

  namespace detail {
    template <typename Head, typename... Tail>
    std::tuple<Tail...> skip_first_type_impl(std::tuple<Head, Tail...> const&);

    template <typename Head, typename... Tail>
    std::tuple<Tail...> skip_first_type_impl(std::pair<Head, Tail...> const&);
  }

  template <typename Tuple>
  using skip_first_type = decltype(detail::skip_first_type_impl(std::declval<Tuple>()));

  template <typename T, typename... Args>
  struct check_parameters {
    using input_parameters = function_parameter_types<T>;
    static_assert(std::tuple_size<input_parameters>{} >= sizeof...(Args));

    template <std::size_t... Is>
    static constexpr bool check_params_for(std::index_sequence<Is...>)
    {
      return std::conjunction_v<std::is_same<std::tuple_element_t<Is, input_parameters>, Args>...>;
    }

    constexpr operator bool() noexcept { return value; }
    static constexpr bool value = check_params_for(std::index_sequence_for<Args...>{});
  };

  // ===================================================================
  template <typename T>
  struct is_non_const_lvalue_reference : std::is_lvalue_reference<T> {};

  template <typename T>
  struct is_non_const_lvalue_reference<T const&> : std::false_type {};

  template <typename T>
  class remove_atomic {
  public:
    using type = T;
  };

  template <typename T>
  class remove_atomic<std::atomic<T>> {
  public:
    using type = T;
  };

  template <typename T>
  using remove_atomic_t = remove_atomic<T>::type;

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
}

#endif // PHLEX_METAPROGRAMMING_TYPE_DEDUCTION_HPP

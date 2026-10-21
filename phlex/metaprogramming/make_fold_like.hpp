#ifndef PHLEX_METAPROGRAMMING_MAKE_FOLD_LIKE_HPP
#define PHLEX_METAPROGRAMMING_MAKE_FOLD_LIKE_HPP

#include "phlex/metaprogramming/type_deduction.hpp"

#include <functional>

namespace phlex::detail {

  template <typename Fn>
  struct make_fold_like_traits {
    // This is an std::tuple, so that's the list type that gets used when this is further transformed
    using all_args_t = function_parameter_types<Fn>;
    using num_accumulators =
      boost::mp11::mp_find_if<all_args_t,
                              boost::mp11::mp_not_fn<is_non_const_lvalue_reference>::fn>;
    static_assert(boost::mp11::mp_size<all_args_t>::value > num_accumulators::value,
                  "A fold function must take at least one input argument");

    using accumulators_t = boost::mp11::mp_take<all_args_t, num_accumulators>;
    using accumulator_tuple_t =
      boost::mp11::mp_apply<std::tuple,
                            boost::mp11::mp_transform<std::remove_reference_t, accumulators_t>>;
    using input_args_t = boost::mp11::mp_drop<all_args_t, num_accumulators>;
  };

  template <typename Fn, typename Args>
  class make_fold_like_impl;
  template <typename Fn, typename... Args>
  class make_fold_like_impl<Fn, std::tuple<Args...>> {
  public:
    static_assert(
      std::is_same_v<typename make_fold_like_traits<Fn>::input_args_t, std::tuple<Args...>>,
      "The second template argument to make_fold_like_impl must be a tuple of the input argument "
      "types");
    make_fold_like_impl(Fn f) : f_(std::move(f)) {}

    return_type<Fn> operator()(make_fold_like_traits<Fn>::accumulator_tuple_t& acc, Args... args)
    {
      auto bound = std::apply(
        [this](auto&&... acc_args) mutable -> decltype(auto) {
          return std::bind_front(std::ref(f_), std::ref(acc_args)...);
        },
        acc);
      return std::invoke(bound, std::forward<Args>(args)...);
    }

  private:
    Fn f_;
  };

  template <typename Fn>
  make_fold_like_impl(Fn f)
    -> make_fold_like_impl<Fn, typename make_fold_like_traits<Fn>::input_args_t>;

  /// Convert a callable that takes M non const lvalue references and N other (input) arguments
  /// into a callable that takes one non const lvalue reference to a tuple and N input arguments.
  /// The non const lvalue references must be the *first* M arguments
  ///
  template <typename F>
  decltype(auto) make_fold_like(F&& f)
  {
    using namespace boost::mp11;
    if constexpr (std::is_member_function_pointer_v<F> ||
                  make_fold_like_traits<F>::num_accumulators::value < 2) {
      return std::forward<F>(f);
    } else {
      return make_fold_like_impl(std::forward<F>(f));
    }
  }
}

#endif // PHLEX_METAPROGRAMMING_MAKE_FOLD_LIKE_HPP

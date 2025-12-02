#ifndef PHLEX_METAPROGRAMMING_DETAIL_CTOR_REFLECT_TYPES_HPP
#define PHLEX_METAPROGRAMMING_DETAIL_CTOR_REFLECT_TYPES_HPP

#include "boost/predef.h"

#if BOOST_COMP_GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

#include <concepts>
#include <tuple>
#include <utility>

// Insane amount of metaprogramming created by Alexandr Poltavsky, motivated by work done
// by Antony Polukhin.  This makes it possible to deduce constructor arguments, relying on
// a loophole in the C++ type system.  Presumably, once reflection is available, we won't
// have to resort to these shenanigans.

// NOLINTBEGIN(cert-dcl50-cpp)

// Begin Alexandr code
// ===========================================================================

// Based on
// * http://alexpolt.github.io/type-loophole.html
//   https://github.com/alexpolt/luple/blob/master/type-loophole.h
//   by Alexandr Poltavsky, http://alexpolt.github.io
// * https://www.youtube.com/watch?v=UlNUNxLtBI0
//   Better C++14 reflections - Antony Polukhin - Meeting C++ 2018

namespace refl {

  // tag<T, N> generates friend declarations and helps with overload resolution.  There
  // are two types: one with the auto return type, which is the way we read types later.
  // The second one is used in the detection of instantiations without which we'd get
  // multiple definitions.
  template <typename T, int N>
  struct tag {
    friend auto loophole(tag<T, N>);
    constexpr friend int cloophole(tag<T, N>);
  };

  // The definitions of friend functions.
  template <typename T,
            typename U,
            int N,
            bool B,
            typename =
              std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>,
                                               std::remove_cv_t<std::remove_reference_t<U>>>>>
  struct fn_def {
    friend auto loophole(tag<T, N>) { return U{}; }
    constexpr friend int cloophole(tag<T, N>) { return 0; }
  };

  // This specialization is to avoid multiple definition errors.
  template <typename T, typename U, int N>
  struct fn_def<T, U, N, true> {};

  // This has a templated conversion operator which in turn triggers instantiations.
  // Important point, using sizeof seems to be more reliable. Also default template
  // arguments are "cached" (I think). To fix that I provide a U template parameter to the
  // ins functions which do the detection using constexpr friend functions and SFINAE.
  template <typename T, int N>
  struct c_op {
    template <typename U, int M>
    static auto ins(...) -> int;
    template <typename U, int M, int = cloophole(tag<T, M>{})>
    static auto ins(int) -> char;

    template <typename U, int = sizeof(fn_def<T, U, N, sizeof(ins<U, N>(0)) == sizeof(char)>)>
    operator U();
  };

  // Here we detect the data type field number. The byproduct is instantiations.  Uses
  // list initialization. Won't work for types with user-provided constructors.  In C++17
  // there is std::is_aggregate which can be added later.
  template <typename T, int... Ns>
  constexpr int fields_number(...)
  {
    return sizeof...(Ns) - 1;
  }

  template <typename T, int... Ns>
  constexpr auto fields_number(int) -> decltype(T{c_op<T, Ns>{}...}, 0)
  {
    return fields_number<T, Ns..., sizeof...(Ns)>(0);
  }

  // Here is a version of fields_number to handle user-provided ctor.
  // NOTE: It finds the first ctor having the shortest unambigious set of parameters.
  template <typename T, int... Ns>
  constexpr auto fields_number_ctor(int) -> decltype(T(c_op<T, Ns>{}...), 0)
  {
    return sizeof...(Ns);
  }

  template <typename T, int... Ns>
  constexpr int fields_number_ctor(...)
  {
    return fields_number_ctor<T, Ns..., sizeof...(Ns)>(0);
  }

  // This is a helper to turn a ctor into a tuple type.  Usage is: refl::as_tuple<data_t>
  template <typename T, typename U>
  struct loophole_tuple;

  template <typename T, int... Ns>
  struct loophole_tuple<T, std::integer_sequence<int, Ns...>> {
    using type = std::tuple<decltype(loophole(tag<T, Ns>{}))...>;
  };

  template <typename T>
  using as_tuple =
    typename loophole_tuple<T, std::make_integer_sequence<int, fields_number_ctor<T>(0)>>::type;

} // namespace refl

// ===========================================================================
// End Alexandr code

// NOLINTEND(cert-dcl50-cpp)

#if BOOST_COMP_GNUC
#pragma GCC diagnostic pop
#endif

#endif // PHLEX_METAPROGRAMMING_DETAIL_CTOR_REFLECT_TYPES_HPP

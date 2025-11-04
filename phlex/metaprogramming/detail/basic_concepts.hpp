#ifndef PHLEX_METAPROGRAMMING_DETAIL_BASIC_CONCEPTS_HPP
#define PHLEX_METAPROGRAMMING_DETAIL_BASIC_CONCEPTS_HPP

namespace phlex::experimental::detail {
  template <typename T>
  concept has_call_operator = requires { &T::operator(); };
}

#endif // PHLEX_METAPROGRAMMING_DETAIL_BASIC_CONCEPTS_HPP

#ifndef phlex_utilities_sized_tuple_hpp
#define phlex_utilities_sized_tuple_hpp

#include <cstddef>
#include <tuple>
#include <utility>

namespace phlex::experimental {
  // Infrastructure to allow specification of (e.g.) sized_tuple<T, 4>, which is an alias
  // for std::tuple<T, T, T, T>
  template <typename T, std::size_t>
  using type_t = T;

  template <typename T, typename>
  using type_for_t = T;

  template <typename T, std::size_t... I>
  std::tuple<type_t<T, I>...> sized_tuple_for(std::index_sequence<I...>);

  template <typename T, std::size_t N>
  using sized_tuple = decltype(sized_tuple_for<T>(std::make_index_sequence<N>{}));

  template <typename... T>
  using concatenated_tuples = decltype(std::tuple_cat(std::declval<T>()...));
}

#endif // phlex_utilities_sized_tuple_hpp

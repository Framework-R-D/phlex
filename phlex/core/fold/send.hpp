#ifndef PHLEX_CORE_FOLD_SEND_HPP
#define PHLEX_CORE_FOLD_SEND_HPP

// =======================================================================================
// Phlex requires fold results to be "sendable", where the result can be represented
// as a data product that is at least moveable.  For some types, notably std::atomic<T>
// specializations, move operations are not supported.  The framework thus permits a
// translation step where a non-moveable type can be converted to a type that is moveable.
//
// For non-moveable types, an overload of 'T send(U const&)' is required, where U is the
// unmoveable type, and T is the type that can be a represented as a data product.
//
// If a type both has a 'send(...)' overload and is move-constructible, the 'send(...)'
// overload takes precedence.
// =======================================================================================

#include <boost/mp11/tuple.hpp>

#include <atomic>
#include <concepts>

namespace phlex::experimental {
  template <typename T>
  T send(std::atomic<T> const& a)
  {
    return a.load();
  }

  namespace detail {
    template <typename T>
    struct sendable_type_impl {};
  }

  template <typename T>
  using sendable_type = detail::sendable_type_impl<T>::type;

  /// A tuple of sendable types is sendable
  template <typename... Ts>
    requires(requires { typename sendable_type<Ts>; } && ...)
  std::tuple<sendable_type<Ts>...> send(std::tuple<Ts...> const& tup);

  template <typename T>
  concept has_send = requires(T const& t) {
    { send(t) } -> std::move_constructible;
  };

  template <typename T>
  concept sendable_by_move = std::move_constructible<T> && !has_send<T>;

  namespace detail {
    template <has_send T>
    struct sendable_type_impl<T> {
      using type = decltype(send(std::declval<T const&>()));
    };

    template <sendable_by_move T>
    struct sendable_type_impl<T> {
      using type = T;
    };
  }

  template <typename... Ts>
    requires(requires { typename sendable_type<Ts>; } && ...)
  std::tuple<sendable_type<Ts>...> send(std::tuple<Ts...> const& tup)
  {
    constexpr auto f = [](auto const& arg) {
      if constexpr (has_send<decltype(arg)>) {
        return send(arg);
      } else {
        return std::move(arg);
      }
    };

    return boost::mp11::tuple_transform(f, tup);
  }
}

#endif // PHLEX_CORE_FOLD_SEND_HPP

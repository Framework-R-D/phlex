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

#include <atomic>
#include <concepts>

namespace phlex::experimental {
  template <typename T>
  T send(std::atomic<T> const& a)
  {
    return a.load();
  }

  template <typename T>
  concept has_send = requires(T const& t) {
    { send(t) } -> std::move_constructible;
  };

  template <typename T>
  concept move_constructible_only = std::move_constructible<T> && !has_send<T>;

  namespace detail {
    template <typename T>
    struct sendable_type_impl {};

    template <has_send T>
    struct sendable_type_impl<T> {
      using type = decltype(send(std::declval<T const&>()));
    };

    template <move_constructible_only T>
    struct sendable_type_impl<T> {
      using type = T;
    };
  }

  template <typename T>
  using sendable_type = typename detail::sendable_type_impl<T>::type;
}

#endif // PHLEX_CORE_FOLD_SEND_HPP

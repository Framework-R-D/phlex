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
  struct sendable_type_impl {};

  template <std::move_constructible T>
  struct sendable_type_impl<T> {
    using type = T;
  };

  template <typename T>
    requires requires(T const& t) {
      { send(t) } -> std::move_constructible;
    }
  struct sendable_type_impl<T> {
    using type = decltype(send(std::declval<T>()));
  };

  template <typename T>
  using sendable_type = typename sendable_type_impl<T>::type;
}

#endif // PHLEX_CORE_FOLD_SEND_HPP

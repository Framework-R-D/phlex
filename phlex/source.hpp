#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "boost/dll/alias.hpp"

#include "phlex/configuration.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/model/product_store.hpp"

#include <concepts>
#include <memory>

namespace phlex::experimental::detail {

  // See note below.
  template <typename T>
  auto make(configuration const& config)
  {
    if constexpr (requires { T{config}; }) {
      return std::make_shared<T>(config);
    } else {
      return std::make_shared<T>();
    }
  }

  template <typename T>
  concept next_function_with_driver = requires(T t, framework_driver& driver) {
    { t.next(driver) } -> std::same_as<void>;
  };

  template <typename T>
  concept next_function_without_driver = requires(T t) {
    { t.next() } -> std::same_as<void>;
  };

  // Workaround for static_assert(false) until P2593R1 is adopted
  //   https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2593r1.html
  // static_assert(false) is supported in GCC 13 and newer
  template <typename T>
  constexpr bool always_false{false};

  template <typename T>
  std::function<void(framework_driver&)> create_next(configuration const& config = {})
  {
    // N.B. Because we are initializing an std::function object with a lambda, the lambda
    //      (and therefore its captured values) must be copy-constructible.  This means
    //      that make<T>(config) must return a copy-constructible object.  Because we do not
    //      know if a user's provided source class is copyable, we create the object on
    //      the heap, and capture a shared pointer to the object.  This also ensures that
    //      the source object is created only once, thus avoiding potential errors in the
    //      implementations of the source class' copy/move constructors (e.g. if the
    //      source is caching an iterator).
    if constexpr (next_function_with_driver<T>) {
      return [t = make<T>(config)](framework_driver& driver) { t->next(driver); };
    } else if constexpr (next_function_without_driver<T>) {
      return [t = make<T>(config)](framework_driver&) { t->next(); };
    } else {
      static_assert(always_false<T>, "Must have a 'next()' function that returns 'void'");
    }
  }

  using next_store_t = std::function<void(framework_driver&)>;
  using source_creator_t = next_store_t(configuration const&);
}

#define PHLEX_EXPERIMENTAL_REGISTER_SOURCE(source)                                                 \
  BOOST_DLL_ALIAS(phlex::experimental::detail::create_next<source>, create_source)

#endif // PHLEX_SOURCE_HPP

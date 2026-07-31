#ifndef PHLEX_METAPROGRAMMING_DELEGATE_HPP
#define PHLEX_METAPROGRAMMING_DELEGATE_HPP

#include "phlex/metaprogramming/type_deduction.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace phlex::detail {
  // The first overload is used for closure objects and free functions
  // The clang-tidy warning that 'auto f' should become 'auto const& f' is a false positive.
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto delegate(std::shared_ptr<void_tag> const&, auto f) { return std::function{std::move(f)}; }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T> obj, R (T::*f)(Args...))
  {
    return std::function{[t = std::move(obj), f](Args... args) mutable -> R {
      return std::invoke(f, *t, std::forward<Args>(args)...);
    }};
  }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T> obj, R (T::*f)(Args...) const)
  {
    return std::function{[t = std::move(obj), f](Args... args) mutable -> R {
      return std::invoke(f, *t, std::forward<Args>(args)...);
    }};
  }

  template <typename Bound, typename Algorithm>
  class algorithm_bits {
  public:
    using bound_type = Bound;
    using algorithm_type = Algorithm;
    using input_parameter_types = function_parameter_types<Algorithm>;
    static constexpr auto number_inputs = std::tuple_size_v<input_parameter_types>;

    // A single templated constructor handles both cases: 'object' deduces to
    // shared_ptr<void_tag> for free functions and closures, and to shared_ptr<T>
    // for stateful objects.  The 'delegate' overload set performs the dispatch, so
    // no separate code paths are needed here.  Both parameters are by-value sinks
    // (moved into 'delegate'), so the clang-tidy const-reference warning is a false
    // positive.
    template <typename T>
    algorithm_bits(T object, Algorithm algorithm) : // NOLINT(performance-unnecessary-value-param)
      bound_{delegate(std::move(object), std::move(algorithm))}
    {
    }

    auto release_algorithm() { return std::move(bound_); }

  private:
    Bound bound_;
  };

  template <typename T, typename Algorithm>
  algorithm_bits(std::shared_ptr<T>, Algorithm)
    -> algorithm_bits<decltype(delegate(std::shared_ptr<T>{}, std::declval<Algorithm>())),
                      Algorithm>;

}

#endif // PHLEX_METAPROGRAMMING_DELEGATE_HPP

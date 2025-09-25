#ifndef phlex_metaprogramming_delegate_hpp
#define phlex_metaprogramming_delegate_hpp

#include "phlex/metaprogramming/type_deduction.hpp"

#include <functional>
#include <memory>

namespace phlex::experimental {
  struct void_tag {};

  template <typename FT>
  auto delegate(std::shared_ptr<void_tag>, FT f) // Used for lambda closures
  {
    return std::function{f};
  }

  template <typename R, typename... Args>
  auto delegate(std::shared_ptr<void_tag>, R (*f)(Args...))
  {
    return std::function{f};
  }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T> obj, R (T::*f)(Args...))
  {
    return std::function{[t = obj, f](Args... args) mutable -> R { return ((*t).*f)(args...); }};
  }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T> obj, R (T::*f)(Args...) const)
  {
    return std::function{[t = obj, f](Args... args) mutable -> R { return ((*t).*f)(args...); }};
  }

  template <typename Bound, typename Algorithm>
  class algorithm_bits {
  public:
    using bound_type = Bound;
    using algorithm_type = Algorithm;
    using input_parameter_types = function_parameter_types<Algorithm>;
    static constexpr auto number_inputs = std::tuple_size_v<input_parameter_types>;

    template <typename T>
    algorithm_bits(T object, Algorithm algorithm) :
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

#endif // phlex_metaprogramming_delegate_hpp

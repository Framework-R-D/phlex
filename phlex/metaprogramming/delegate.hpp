#ifndef phlex_metaprogramming_delegate_hpp
#define phlex_metaprogramming_delegate_hpp

#include <functional>
#include <memory>

namespace phlex::experimental {
  struct void_tag {};
  struct unfold_tag {};

  template <typename FT>
  auto delegate(std::shared_ptr<void_tag>&, FT f) // Used for lambda closures
  {
    return std::function{f};
  }

  template <typename R, typename... Args>
  auto delegate(std::shared_ptr<void_tag>&, R (*f)(Args...))
  {
    return std::function{f};
  }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T>& obj, R (T::*f)(Args...))
  {
    return std::function{[t = obj, f](Args... args) mutable -> R { return ((*t).*f)(args...); }};
  }

  template <typename R, typename T, typename... Args>
  auto delegate(std::shared_ptr<T>& obj, R (T::*f)(Args...) const)
  {
    return std::function{[t = obj, f](Args... args) mutable -> R { return ((*t).*f)(args...); }};
  }
}

#endif // phlex_metaprogramming_delegate_hpp

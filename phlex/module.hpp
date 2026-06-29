#ifndef PHLEX_MODULE_HPP
#define PHLEX_MODULE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::experimental {
  /// @brief Proxy for registering module algorithm nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_ALGORITHMS plugin entry points. Provides
  /// access to fold, observe, output, predicate, transform, and unfold registration.
  /// Users never construct this type directly.
  template <typename T>
  class module_graph_proxy : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    using base::graph_proxy;

    template <typename U, typename... Args>
    module_graph_proxy<U> make(Args&&... args)
      requires(not is_bound_object<T>)
    {
      return this->template bind_to<module_graph_proxy, U>(std::forward<Args>(args)...);
    }

    using base::fold;
    using base::observe;
    using base::output;
    using base::predicate;
    using base::transform;
    using base::unfold;
  };

  namespace detail {
    using module_creator_t = void(module_graph_proxy<void_tag>, configuration const&);
  }
}

#define PHLEX_REGISTER_ALGORITHMS(...)                                                             \
  PHLEX_DETAIL_REGISTER_PLUGIN(                                                                    \
    phlex::experimental::module_graph_proxy, create, create_module, __VA_ARGS__)

#endif // PHLEX_MODULE_HPP

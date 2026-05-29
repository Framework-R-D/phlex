#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::experimental {
  /// @brief Proxy for registering source (provider) nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_PROVIDERS plugin entry points. Only provide
  /// registration is accessible. Users never construct this type directly.
  template <typename T, bool BoundObject = false>
  class source_graph_proxy : graph_proxy<T, BoundObject> {
    using base = graph_proxy<T, BoundObject>;

  public:
    using base::graph_proxy;

    template <typename U, typename... Args>
    source_graph_proxy<U, true> make(Args&&... args)
      requires(not BoundObject)
    {
      return this->template bind_to<source_graph_proxy, U>(std::forward<Args>(args)...);
    }

    // Only provide(...) should be accessible
    using base::provide;
  };

  namespace detail {
    using source_creator_t = void(source_graph_proxy<void_tag>, configuration const&);
  }
}

#define PHLEX_REGISTER_PROVIDERS(...)                                                              \
  PHLEX_DETAIL_REGISTER_PLUGIN(                                                                    \
    phlex::experimental::source_graph_proxy, create, create_source, __VA_ARGS__)

#endif // PHLEX_SOURCE_HPP

#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::experimental {

  struct source_bundle {
    configuration const& config;
    tbb::flow::graph& graph;
    node_catalog& nodes;
    std::vector<std::string>& registration_errors;
  };

  /// @brief Proxy for registering explicit provider nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_PROVIDERS plugin entry points. Only provide
  /// registration is accessible. Users never construct this type directly.
  template <typename T>
  class providers_graph_proxy : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    providers_graph_proxy(source_bundle bundle) :
      base{bundle.config, bundle.graph, bundle.nodes, bundle.registration_errors}
    {
    }

    using base::graph_proxy;

    template <typename U, typename... Args>
    providers_graph_proxy<U> make(Args&&... args)
      requires(not is_bound_object<T>)
    {
      return this->template bind_to<providers_graph_proxy, U>(std::forward<Args>(args)...);
    }

    // Only provide(...) should be accessible
    using base::provide;
  };

  /// @brief Proxy for registering source nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_SOURCE plugin entry points. Only source
  /// registration is accessible. Users never construct this type directly.
  template <typename T>
  class source_graph_proxy : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    source_graph_proxy(source_bundle bundle) :
      base{bundle.config, bundle.graph, bundle.nodes, bundle.registration_errors}
    {
    }

    // Only source(...) should be accessible
    using base::source;
  };

  namespace detail {
    using source_creator_t = void(source_bundle, configuration const&);
  }
}

#define PHLEX_REGISTER_PROVIDERS(...)                                                              \
  PHLEX_DETAIL_REGISTER_SOURCE_PLUGIN(                                                             \
    phlex::experimental::providers_graph_proxy, create, create_source, __VA_ARGS__)

#define PHLEX_REGISTER_SOURCE(...)                                                                 \
  PHLEX_DETAIL_REGISTER_SOURCE_PLUGIN(                                                             \
    phlex::experimental::source_graph_proxy, create, create_source, __VA_ARGS__)

#endif // PHLEX_SOURCE_HPP

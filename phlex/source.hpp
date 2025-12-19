#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "boost/dll/alias.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

namespace phlex::experimental {
  template <typename T>
  class source_graph_proxy : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    using base::graph_proxy;

    // FIXME: make sure functions called from make<T>(...) are restricted to the functions below:
    //        Users can call make<T>(...).provide(...) but not make<T>(...).fold(...)
    using base::make;

    // Only provide(...) should be accessible
    using base::provide;
  };

  namespace detail {
    using source_creator_t = void(source_graph_proxy<void_tag>, configuration const&);
  }
}

#define PHLEX_EXPERIMENTAL_REGISTER_PROVIDERS(...)                                                 \
  PHLEX_DETAIL_REGISTER_PLUGIN(                                                                    \
    phlex::experimental::source_graph_proxy, create, create_source, __VA_ARGS__)

#endif // PHLEX_SOURCE_HPP

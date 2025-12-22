#ifndef PHLEX_MODULE_HPP
#define PHLEX_MODULE_HPP

#include "boost/dll/alias.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

namespace phlex::experimental {
  template <typename T>
  class module_graph_proxy : graph_proxy<T> {
    using base = graph_proxy<T>;

  public:
    using base::graph_proxy;

    // FIXME: make sure functions called from make<T>(...) are restricted to the functions below:
    //        Users can call make<T>(...).fold(...) but not make<T>(...).provide(...)
    using base::make;

    using base::fold;
    using base::observe;
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

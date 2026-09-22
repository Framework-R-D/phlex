#ifndef PHLEX_RESOURCE_HPP
#define PHLEX_RESOURCE_HPP

#include "phlex/core/graph_proxy.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::detail {

  /// @brief Proxy for registering resources.
  ///
  /// Passed to @c PHLEX_REGISTER_RESOURCES plugin entry points. Only resource
  /// registration is accessible. Users never construct this type directly.
  template <typename T>
  class resources_graph_proxy {
  public:
    explicit resources_graph_proxy(graph_registration_bundle bundle) : resources_{bundle.resources}
    {
    }

    template <typename Resource, typename... Args>
      requires unlimited_resource_registration<Resource, Args...>
    void add_unlimited_resource(Args&&... args) const
    {
      resources_.template add_unlimited<Resource>(std::forward<Args>(args)...);
    }

    template <typename Resource, typename... Args>
      requires serialized_resource_registration<Resource, Args...>
    void add_serialized_resource(Args&&... args) const
    {
      resources_.template add_serialized<Resource>(std::forward<Args>(args)...);
    }

  private:
    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  namespace internal {
    using resource_creator_t = void(resources_graph_proxy<void_tag> const&, configuration const&);
  }
}

#define PHLEX_REGISTER_RESOURCES(...)                                                              \
  PHLEX_DETAIL_REGISTER_PLUGIN(                                                                    \
    phlex::detail::resources_graph_proxy, create, create_resources, __VA_ARGS__)

#endif // PHLEX_RESOURCE_HPP

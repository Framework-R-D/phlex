#ifndef PHLEX_RESOURCE_HPP
#define PHLEX_RESOURCE_HPP

#include "phlex/core/registration_context.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::detail {

  /// @brief Proxy for registering resources.
  ///
  /// Passed to @c PHLEX_REGISTER_RESOURCES plugin entry points. Only resource
  /// registration is accessible. Users never construct this type directly or retain it beyond the
  /// synchronous callback.
  class resources_graph_proxy {
  public:
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
    friend class internal::proxy_factory;

    explicit resources_graph_proxy(resource_catalog& resources) : resources_{resources} {}

    resource_catalog& resources_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  namespace internal {
    using resource_creator_t = void(resources_graph_proxy const&, configuration const&);

    inline resources_graph_proxy proxy_factory::resources_proxy(registration_context context)
    {
      return resources_graph_proxy{*context.resources_};
    }
  }
}

#define PHLEX_REGISTER_RESOURCES(...)                                                              \
  PHLEX_DETAIL_REGISTER_NONTEMPLATE_PLUGIN(                                                        \
    phlex::detail::resources_graph_proxy, create, create_resources, __VA_ARGS__)

#endif // PHLEX_RESOURCE_HPP

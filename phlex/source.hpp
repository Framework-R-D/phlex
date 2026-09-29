#ifndef PHLEX_SOURCE_HPP
#define PHLEX_SOURCE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/registration_context.hpp"
#include "phlex/core/source.hpp"
#include "phlex/detail/plugin_macros.hpp"

#include <utility>

namespace phlex::detail {
  /// @brief Proxy for registering explicit provider nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_PROVIDERS plugin entry points. Only provide
  /// registration is accessible. Users never construct this type directly.
  template <typename T>
  class providers_graph_proxy {
  public:
    template <typename U, typename... Args>
    providers_graph_proxy<U> make(Args&&... args) const
      requires(not is_bound_object<T>)
    {
      return providers_graph_proxy<U>{state_.template bind<U>(std::forward<Args>(args)...)};
    }

    auto provide(std::string_view name,
                 is_provider_like auto f,
                 concurrency c = concurrency::serial) const
    {
      return state_.make_glue().provide(name, std::move(f), c);
    }

  private:
    friend class internal::proxy_factory;
    template <typename>
    friend class providers_graph_proxy;

    explicit providers_graph_proxy(internal::registration_state<T> state) : state_{std::move(state)}
    {
    }

    internal::registration_state<T> state_;
  };

  /// @brief Proxy for registering source nodes.
  ///
  /// Passed to @c PHLEX_REGISTER_SOURCE plugin entry points. Only source
  /// registration is accessible. Users never construct this type directly.
  class source_graph_proxy {
  public:
    template <std::derived_from<source> Source, typename... Args>
    void add_source(std::string_view name, Args&&... args) const
    {
      state_.make_glue(false).template add_source<Source>(name, std::forward<Args>(args)...);
    }

  private:
    friend class internal::proxy_factory;

    explicit source_graph_proxy(internal::registration_state<void_tag> state) :
      state_{std::move(state)}
    {
    }

    internal::registration_state<void_tag> state_;
  };

  namespace internal {
    using source_creator_t = void(registration_carrier, configuration const&);
  }

  inline providers_graph_proxy<void_tag> internal::proxy_factory::providers_proxy(
    registration_carrier carrier)
  {
    return providers_graph_proxy<void_tag>{registration_state<void_tag>{carrier.context_}};
  }

  inline source_graph_proxy internal::proxy_factory::source_proxy(registration_carrier carrier)
  {
    return source_graph_proxy{registration_state<void_tag>{carrier.context_}};
  }
}

#define PHLEX_REGISTER_PROVIDERS(...)                                                              \
  PHLEX_DETAIL_REGISTER_SOURCE_PLUGIN(                                                             \
    phlex::detail::providers_graph_proxy, create, create_source, __VA_ARGS__)

#define PHLEX_REGISTER_SOURCE(...)                                                                 \
  PHLEX_DETAIL_REGISTER_NONTEMPLATE_SOURCE_PLUGIN(                                                 \
    phlex::detail::source_graph_proxy, create, create_source, __VA_ARGS__)

#endif // PHLEX_SOURCE_HPP

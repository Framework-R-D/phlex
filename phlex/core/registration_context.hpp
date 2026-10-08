#ifndef PHLEX_CORE_REGISTRATION_CONTEXT_HPP
#define PHLEX_CORE_REGISTRATION_CONTEXT_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/glue.hpp"

#include <memory>
#include <utility>

namespace phlex::detail {
  class framework_graph;
  template <typename>
  class module_graph_proxy;
  template <typename>
  class providers_graph_proxy;
  class source_graph_proxy;
  class resources_graph_proxy;

  namespace internal {
    class proxy_factory;

    /// Non-owning registration state whose lifetime is limited to a synchronous plugin callback.
    class registration_context {
    public:
      registration_context(registration_context const&) = default;

    private:
      friend class phlex::detail::framework_graph;
      friend class proxy_factory;
      template <typename>
      friend class registration_state;

      registration_context(configuration const* config,
                           tbb::flow::graph& graph,
                           phlex::experimental::identifier const& stage,
                           node_catalog& nodes,
                           std::vector<std::string>& errors,
                           resource_catalog& resources) :
        config_{config},
        graph_{&graph},
        stage_{&stage},
        nodes_{&nodes},
        errors_{&errors},
        resources_{&resources}
      {
      }

      configuration const* config_;
      tbb::flow::graph* graph_;
      phlex::experimental::identifier const* stage_;
      node_catalog* nodes_;
      std::vector<std::string>* errors_;
      resource_catalog* resources_;
    };

    /// Internal glue-construction state. It is not a user-facing registration facade.
    template <typename T>
    class registration_state {
    public:
      template <typename U, typename... Args>
      registration_state<U> bind(Args&&... args) const
        requires(not is_bound_object<T>)
      {
        return {context_, std::make_shared<U>(std::forward<Args>(args)...)};
      }

      glue<T> make_glue(bool use_bound_object = true) const
      {
        return {registration_core{context_.config_,
                                  *context_.graph_,
                                  *context_.stage_,
                                  *context_.nodes_,
                                  *context_.errors_,
                                  *context_.resources_},
                use_bound_object ? bound_object_ : nullptr};
      }

      template <typename U>
      glue<U> make_unbound_glue() const
      {
        return {registration_core{context_.config_,
                                  *context_.graph_,
                                  *context_.stage_,
                                  *context_.nodes_,
                                  *context_.errors_,
                                  *context_.resources_},
                nullptr};
      }

    private:
      friend class phlex::detail::framework_graph;
      friend class proxy_factory;
      template <typename>
      friend class registration_state;

      registration_state(registration_context context, std::shared_ptr<T> bound_object = nullptr) :
        context_{context}, bound_object_{std::move(bound_object)}
      {
      }

      registration_context context_;
      std::shared_ptr<T> bound_object_;
    };

    /// Opaque carrier shared by source and explicit-provider plugin entry points.
    class registration_carrier {
    public:
      registration_carrier(registration_carrier const&) = default;

    private:
      friend class phlex::detail::framework_graph;
      friend class proxy_factory;

      explicit registration_carrier(registration_context context) : context_{context} {}

      registration_context context_;
    };

    /// The single construction point for capability-specific registration proxies.
    class proxy_factory {
    public:
      static module_graph_proxy<void_tag> module_proxy(registration_context context);
      static providers_graph_proxy<void_tag> providers_proxy(registration_carrier carrier);
      static source_graph_proxy source_proxy(registration_carrier carrier);
      static resources_graph_proxy resources_proxy(registration_context context);
    };
  }
}

#endif // PHLEX_CORE_REGISTRATION_CONTEXT_HPP

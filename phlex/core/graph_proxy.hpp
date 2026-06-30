#ifndef PHLEX_CORE_GRAPH_PROXY_HPP
#define PHLEX_CORE_GRAPH_PROXY_HPP

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/metaprogramming/delegate.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phlex {
  class configuration;
}

namespace phlex::detail {
  // ==============================================================================
  // Registering user functions

  /// @brief Proxy for registering algorithm nodes in a processing graph.
  ///
  /// Passed to user plugin entry points by the framework. Use the registration
  /// methods to attach user algorithms to the graph. Users never construct
  /// this type directly.
  template <typename T>
  class graph_proxy {
  public:
    template <typename>
    friend class graph_proxy;

    graph_proxy(configuration const& config,
                tbb::flow::graph& g,
                node_catalog& nodes,
                std::vector<std::string>& errors)
      requires(not is_bound_object<T>)
      : config_{&config}, graph_{g}, nodes_{nodes}, errors_{errors}
    {
    }

    /// @brief Binds a user algorithm object of type @p U to this proxy.
    ///
    /// Constructs an instance of @p U forwarding @p args to its constructor.
    /// Returns a new proxy through which member functions of that object may
    /// be registered as algorithm nodes.
    template <typename U, typename... Args>
    graph_proxy<U> make(Args&&... args)
      requires(not is_bound_object<T>)
    {
      return bind_to<graph_proxy, U>(std::forward<Args>(args)...);
    }

    /// @brief Registers a fold algorithm node.
    template <typename... InitArgs>
    auto fold(std::string name,
              is_fold_like auto f,
              concurrency c = concurrency::serial,
              std::string partition = "job",
              InitArgs&&... init_args)
    {
      return create_glue().fold(std::move(name),
                                std::move(f),
                                c,
                                std::move(partition),
                                std::forward<InitArgs>(init_args)...);
    }

    /// @brief Registers an observer node.
    auto observe(std::string name, is_observer_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().observe(std::move(name), std::move(f), c);
    }

    /// @brief Registers a predicate node.
    auto predicate(std::string name, is_predicate_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().predicate(std::move(name), std::move(f), c);
    }

    /// @brief Registers a provider node.
    auto provide(std::string name, is_provider_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().provide(std::move(name), std::move(f), c);
    }

    /// @brief Registers a transform node.
    auto transform(std::string name, is_transform_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().transform(std::move(name), std::move(f), c);
    }

    /// @brief Registers an unfold node.
    template <typename Splitter>
    auto unfold(std::string name,
                is_predicate_like auto pred,
                auto unf,
                std::string destination_data_layer,
                concurrency c = concurrency::serial)
    {
      return glue<Splitter>{graph_, nodes_, nullptr, errors_, config_}.unfold(
        std::move(name), std::move(pred), std::move(unf), c, std::move(destination_data_layer));
    }

    /// @brief Registers a source (used by the framework to create provider nodes)
    template <std::derived_from<source> Source, typename... Args>
    void add_source(std::string name, Args&&... args)
      requires(not is_bound_object<T>)
    {
      // The bound object is created when invoking source<Source>(...), so we explicitly indicate that
      // no bound object should be used in the create_glue(...) call.
      return create_glue(false).template add_source<Source>(std::move(name),
                                                            std::forward<Args>(args)...);
    }

    /// @brief Registers an output node.
    auto output(std::string name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().output(std::move(name), std::move(f), c);
    }

  protected:
    template <template <typename> typename Proxy, typename U, typename... Args>
    Proxy<U> bind_to(Args&&... args)
      requires(not is_bound_object<T>)
    {
      return Proxy<U>{
        config_, graph_, nodes_, std::make_shared<U>(std::forward<Args>(args)...), errors_};
    }

    graph_proxy(configuration const* config,
                tbb::flow::graph& g,
                node_catalog& nodes,
                std::shared_ptr<T> bound_obj,
                std::vector<std::string>& errors)
      requires(is_bound_object<T>)
      : config_{config}, graph_{g}, nodes_{nodes}, bound_obj_{bound_obj}, errors_{errors}
    {
    }

  private:
    glue<T> create_glue(bool use_bound_object = true)
    {
      return glue{graph_, nodes_, (use_bound_object ? bound_obj_ : nullptr), errors_, config_};
    }

    configuration const* config_;
    // Non-owning references to framework-owned resources; graph_proxy<T> is a
    // short-lived builder.
    tbb::flow::graph& graph_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    node_catalog& nodes_;     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::shared_ptr<T> bound_obj_;
    std::vector<std::string>& errors_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };
}

#endif // PHLEX_CORE_GRAPH_PROXY_HPP

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

namespace phlex::experimental {
  class configuration;
  // ==============================================================================
  // Registering user functions

  template <typename T>
  class graph_proxy {
  public:
    template <typename>
    friend class graph_proxy;

    graph_proxy(configuration const& config,
                tbb::flow::graph& g,
                node_catalog& nodes,
                std::vector<std::string>& errors)
      requires(std::same_as<T, void_tag>)
      : config_{&config}, graph_{g}, nodes_{nodes}, errors_{errors}
    {
    }

    template <typename U, typename... Args>
    graph_proxy<U> make(Args&&... args)
    {
      return graph_proxy<U>{
        config_, graph_, nodes_, std::make_shared<U>(std::forward<Args>(args)...), errors_};
    }

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

    auto observe(std::string name, is_observer_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().observe(std::move(name), std::move(f), c);
    }

    auto predicate(std::string name, is_predicate_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().predicate(std::move(name), std::move(f), c);
    }

    auto transform(std::string name, is_transform_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().transform(std::move(name), std::move(f), c);
    }

    template <typename Splitter>
    auto unfold(std::string name,
                is_predicate_like auto pred,
                auto unf,
                concurrency c = concurrency::serial)
    {
      return create_glue(false).unfold(std::move(name), std::move(pred), std::move(unf), c);
    }

    template <typename Splitter>
    auto unfold(is_predicate_like auto pred, auto unf, concurrency c = concurrency::serial)
    {
      return create_glue(false).unfold(std::move(pred), std::move(unf), c);
    }

    auto output(std::string name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return create_glue().output(std::move(name), std::move(f), c);
    }

  private:
    graph_proxy(configuration const* config,
                tbb::flow::graph& g,
                node_catalog& nodes,
                std::shared_ptr<T> bound_obj,
                std::vector<std::string>& errors)
      requires(not std::same_as<T, void_tag>)
      : config_{config}, graph_{g}, nodes_{nodes}, bound_obj_{bound_obj}, errors_{errors}
    {
    }

    glue<T> create_glue(bool use_bound_object = true)
    {
      return glue{graph_, nodes_, (use_bound_object ? bound_obj_ : nullptr), errors_, config_};
    }

    configuration const* config_;
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::shared_ptr<T> bound_obj_;
    std::vector<std::string>& errors_;
  };
}

#endif // PHLEX_CORE_GRAPH_PROXY_HPP

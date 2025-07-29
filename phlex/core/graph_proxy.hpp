#ifndef phlex_core_graph_proxy_hpp
#define phlex_core_graph_proxy_hpp

#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/node_catalog.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/metaprogramming/delegate.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <concepts>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {

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

    auto with(std::string name, auto f, concurrency c = concurrency::serial)
    {
      return glue{graph_, nodes_, bound_obj_, errors_, config_}.with(name, f, c);
    }

    template <typename Splitter>
    auto with(auto predicate, auto unfold, concurrency c = concurrency::serial)
    {
      return unfold_glue<Splitter>(graph_, nodes_, errors_).declare_unfold(predicate, unfold, c);
    }

    auto output_with(std::string name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return output_creator{
        nodes_.register_output(errors_), config_, name, graph_, delegate(bound_obj_, f), c};
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

    configuration const* config_;
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::shared_ptr<T> bound_obj_;
    std::vector<std::string>& errors_;
  };
}

#endif // phlex_core_graph_proxy_hpp

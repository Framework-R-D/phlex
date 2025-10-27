#ifndef phlex_core_glue_hpp
#define phlex_core_glue_hpp

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/double_bound_function.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/registration_api.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/utilities/stripped_name.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <cassert>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace phlex::experimental {
  class configuration;
  struct node_catalog;

  namespace detail {
    void verify_name(std::string const& name, configuration const* config);
  }

  // ==============================================================================
  // Registering user functions

  template <typename T>
  class glue {
  public:
    glue(tbb::flow::graph& g,
         node_catalog& nodes,
         std::shared_ptr<T> bound_obj,
         std::vector<std::string>& errors,
         configuration const* config = nullptr) :
      graph_{g}, nodes_{nodes}, bound_obj_{std::move(bound_obj)}, errors_{errors}, config_{config}
    {
    }

    auto with(std::string name, auto f, concurrency c = concurrency::serial)
    {
      detail::verify_name(name, config_);
      return bound_function{config_, std::move(name), bound_obj_, f, c, graph_, nodes_, errors_};
    }

    template <typename FT>
    auto observe(std::string name, FT f, concurrency c = concurrency::serial)
    {
      detail::verify_name(name, config_);
      return make_registration<observer_node>(config_,
                                              std::move(name),
                                              algorithm_bits{bound_obj_, std::move(f)},
                                              c,
                                              graph_,
                                              nodes_,
                                              errors_);
    }

    template <typename FT>
    auto predicate(std::string name, FT f, concurrency c = concurrency::serial)
    {
      detail::verify_name(name, config_);
      return make_registration<predicate_node>(config_,
                                               std::move(name),
                                               algorithm_bits{bound_obj_, std::move(f)},
                                               c,
                                               graph_,
                                               nodes_,
                                               errors_);
    }

    auto output_with(std::string name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return output_creator{nodes_.registrar_for<declared_output_ptr>(errors_),
                            config_,
                            std::move(name),
                            graph_,
                            delegate(bound_obj_, f),
                            c};
    }

  private:
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::shared_ptr<T> bound_obj_;
    std::vector<std::string>& errors_;
    configuration const* config_;
  };

  template <typename T>
  class unfold_glue {
  public:
    unfold_glue(tbb::flow::graph& g,
                node_catalog& nodes,
                std::vector<std::string>& errors,
                configuration const* config = nullptr) :
      graph_{g}, nodes_{nodes}, errors_{errors}, config_{config}
    {
    }

    auto declare_unfold(auto predicate, auto unfold, concurrency c)
    {
      return double_bound_function<T, decltype(predicate), decltype(unfold)>{
        config_,
        detail::stripped_name(boost::core::demangle(typeid(T).name())),
        predicate,
        unfold,
        c,
        graph_,
        nodes_,
        errors_};
    }

  private:
    tbb::flow::graph& graph_;
    node_catalog& nodes_;
    std::vector<std::string>& errors_;
    configuration const* config_;
  };
}

#endif // phlex_core_glue_hpp

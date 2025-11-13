#ifndef PHLEX_CORE_GLUE_HPP
#define PHLEX_CORE_GLUE_HPP

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/registration_api.hpp"
#include "phlex/metaprogramming/delegate.hpp"
#include "phlex/utilities/stripped_name.hpp"

#include "boost/core/demangle.hpp"
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

    template <typename... InitArgs>
    auto fold(
      std::string name, auto f, concurrency c, std::string partition, InitArgs&&... init_args)
    {
      detail::verify_name(name, config_);
      return fold_api{config_,
                      std::move(name),
                      algorithm_bits(bound_obj_, std::move(f)),
                      c,
                      graph_,
                      nodes_,
                      errors_,
                      std::move(partition),
                      std::forward<InitArgs>(init_args)...};
    }

    template <typename FT>
    auto observe(std::string name, FT f, concurrency c)
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
    auto transform(std::string name, FT f, concurrency c)
    {
      detail::verify_name(name, config_);
      return make_registration<transform_node>(config_,
                                               std::move(name),
                                               algorithm_bits{bound_obj_, std::move(f)},
                                               c,
                                               graph_,
                                               nodes_,
                                               errors_);
    }

    template <typename FT>
    auto predicate(std::string name, FT f, concurrency c)
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

    auto unfold(std::string name,
                auto predicate,
                auto unfold,
                concurrency c,
                std::string destination_data_layer)
    {
      assert(!bound_obj_);
      detail::verify_name(name, config_);
      return unfold_api<T, decltype(predicate), decltype(unfold)>{
        config_,
        std::move(name),
        std::move(predicate),
        std::move(unfold),
        c,
        graph_,
        nodes_,
        errors_,
        std::move(destination_data_layer)};
    }

    auto unfold(auto pred, auto unf, concurrency c, std::string destination_data_layer)
    {
      return unfold(detail::stripped_name(boost::core::demangle(typeid(T).name())),
                    std::move(pred),
                    std::move(unf),
                    c,
                    std::move(destination_data_layer));
    }

    auto output(std::string name, is_output_like auto f, concurrency c = concurrency::serial)
    {
      return output_api{nodes_.registrar_for<declared_output_ptr>(errors_),
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
}

#endif // PHLEX_CORE_GLUE_HPP

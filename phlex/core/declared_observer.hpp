#ifndef PHLEX_CORE_DECLARED_OBSERVER_HPP
#define PHLEX_CORE_DECLARED_OBSERVER_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace phlex::detail {

  class PHLEX_CORE_EXPORT declared_observer : public products_consumer {
  public:
    declared_observer(phlex::experimental::algorithm_name name,
                      std::vector<std::string> predicates,
                      product_selectors input_products);
    ~declared_observer() override;
  };

  using declared_observer_ptr = std::unique_ptr<declared_observer>;
  using declared_observers = simple_ptr_map<declared_observer_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits, typename... Resources>
  class observer_node : public declared_observer {
    using function_t = AlgorithmBits::bound_type;
    static constexpr auto num_resources = sizeof...(Resources);
    static constexpr auto num_products = AlgorithmBits::number_inputs - num_resources;
    using input_product_types = AlgorithmBits::template input_parameters<num_products>;
    using builder =
      node_builder<messages_t<num_products>, function_t, no_outputs_t, std::tuple<Resources...>>;
    using node_t = builder::node_t;

  public:
    static constexpr auto number_output_products = 0;
    using node_ptr_type = declared_observer_ptr;

    observer_node(phlex::experimental::algorithm_name algo_name,
                  std::size_t concurrency,
                  std::vector<std::string> predicates,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  product_selectors input_products,
                  resource_catalog& resources) :
      declared_observer{std::move(algo_name), std::move(predicates), std::move(input_products)},
      join_{make_join_or_none<num_products>(g, name().to_string(), layers())},
      observer_{builder::make(
        g,
        concurrency,
        resources,
        alg.release_algorithm(),
        [this](function_t const& ft,
               messages_t<num_products> const& messages,
               auto&&... resource_tokens) {
          call(ft, messages, std::make_index_sequence<num_products>{}, resource_tokens...);
          ++calls_;
        })}
    {
      if constexpr (num_products > 1ull) {
        make_edge(join_, observer_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_selector const& input_product) override
    {
      return receiver_for<num_products>(join_, input(), input_product, observer_);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<num_products>(join_, observer_);
    }

    template <std::size_t... Is>
    void call(function_t const& ft,
              messages_t<num_products> const& messages,
              std::index_sequence<Is...>,
              auto&&... resource_tokens)
    {
      if constexpr (num_products == 1ull) {
        std::invoke(ft, std::get<Is>(input_).retrieve(messages)..., resource_tokens...);
      } else {
        std::invoke(
          ft, std::get<Is>(input_).retrieve(std::get<Is>(messages))..., resource_tokens...);
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }

    input_retriever_types<input_product_types> input_{input_arguments<input_product_types>()};
    join_or_none_t<num_products> join_;
    node_t observer_;
    std::atomic<std::size_t> calls_;
  };
}

#endif // PHLEX_CORE_DECLARED_OBSERVER_HPP

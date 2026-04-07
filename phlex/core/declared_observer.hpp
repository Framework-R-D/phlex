#ifndef PHLEX_CORE_DECLARED_OBSERVER_HPP
#define PHLEX_CORE_DECLARED_OBSERVER_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/store_counters.hpp"
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

namespace phlex::experimental {

  class PHLEX_CORE_EXPORT declared_observer : public products_consumer {
  public:
    declared_observer(algorithm_name name,
                      std::vector<std::string> predicates,
                      product_queries input_products,
                      product_registry const& registry);
    virtual ~declared_observer();
  };

  using declared_observer_ptr = std::unique_ptr<declared_observer>;
  using declared_observers = simple_ptr_map<declared_observer_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class observer_node : public declared_observer {
    using input_args = typename AlgorithmBits::input_parameter_types;
    using function_t = typename AlgorithmBits::bound_type;
    static constexpr auto num_inputs = AlgorithmBits::number_inputs;

  public:
    static constexpr auto number_output_products = 0;
    using node_ptr_type = declared_observer_ptr;

    observer_node(algorithm_name name,
                  std::size_t concurrency,
                  std::vector<std::string> predicates,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  product_queries input_products,
                  product_registry const& registry) :
      declared_observer{
        std::move(name), std::move(predicates), std::move(input_products), registry},
      join_{make_join_or_none<num_inputs>(g, full_name(), layers())},
      observer_{g,
                concurrency,
                [this, ft = alg.release_algorithm()](
                  messages_t<num_inputs> const& messages) -> oneapi::tbb::flow::continue_msg {
                  call(ft, messages, std::make_index_sequence<num_inputs>{});
                  ++calls_;
                  return {};
                }}
    {
      if constexpr (num_inputs > 1ull) {
        make_edge(join_, observer_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& input_product) override
    {
      return receiver_for<num_inputs>(join_, input(), input_product, observer_);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<num_inputs>(join_, observer_);
    }

    template <std::size_t... Is>
    void call(function_t const& ft,
              messages_t<num_inputs> const& messages,
              std::index_sequence<Is...>)
    {
      if constexpr (num_inputs == 1ull) {
        std::invoke(ft, std::get<Is>(input_).retrieve(messages)...);
      } else {
        std::invoke(ft, std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }

    input_retriever_types<input_args> input_{input_arguments<input_args>()};
    join_or_none_t<num_inputs> join_;
    tbb::flow::function_node<messages_t<num_inputs>> observer_;
    std::atomic<std::size_t> calls_;
  };
}

#endif // PHLEX_CORE_DECLARED_OBSERVER_HPP

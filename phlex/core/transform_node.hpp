#ifndef PHLEX_CORE_TRANSFORM_NODE_HPP
#define PHLEX_CORE_TRANSFORM_NODE_HPP

// FIXME: Add comments explaining the process.  For each implementation, explain what part
//        of the process a given section of code is addressing.

#include "phlex/core/concepts.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace phlex::detail {

  template <typename AlgorithmBits, typename... Resources>
  class transform_node : public declared_transform {
    using function_t = AlgorithmBits::algorithm_type;

    static constexpr auto num_resources = sizeof...(Resources);
    static constexpr auto num_products = AlgorithmBits::number_inputs - num_resources;
    static constexpr auto num_outputs = AlgorithmBits::number_outputs;
    using input_product_types = AlgorithmBits::template input_parameters<num_products>;
    using builder =
      node_builder<messages_t<num_products>, std::tuple<message>, std::tuple<Resources...>>;
    using node_t = builder::node_t;

  public:
    static constexpr auto number_output_products = num_outputs;

    transform_node(phlex::experimental::algorithm_name algo_name,
                   phlex::experimental::identifier stage,
                   std::size_t concurrency,
                   std::vector<std::string> predicates,
                   tbb::flow::graph& g,
                   AlgorithmBits alg,
                   product_selectors input_products,
                   std::vector<std::string> output,
                   resource_catalog& resources) :
      declared_transform{std::move(algo_name), std::move(predicates), std::move(input_products), g},
      output_{
        to_product_specifications(name(), std::move(output), make_output_type_ids<function_t>())},
      join_{make_join_or_none<num_products>(g, name().to_string(), layers())},
      transform_{builder::make(
        g,
        concurrency,
        resources,
        alg.release_algorithm(),
        [this, stage = std::move(stage)](function_t const& ft,
                                         messages_t<num_products> const& messages,
                                         auto&&... resource_tokens) -> message {
          using namespace phlex::experimental::detail;
          auto const& msg = most_derived(messages);
          auto const& [store, message_id] = std::tie(msg.store, msg.id);

          auto result =
            call(ft, messages, std::make_index_sequence<num_products>{}, resource_tokens...);
          ++calls_;

          products new_products{num_outputs};
          new_products.add_all(output_, std::move(result));
          auto new_store = std::make_shared<phlex::experimental::product_store>(
            store->index(), gsl::not_null{&name()}, gsl::not_null{&stage}, std::move(new_products));

          return {.store = std::move(new_store), .id = message_id};
        })}
    {
      if constexpr (num_products > 1ull) {
        make_edge(join_, transform_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_selector const& input_product) override
    {
      return receiver_for<num_products>(join_, input(), input_product, transform_);
    }

    tbb::flow::sender<message>& output_port() override { return builder::output_port(transform_); }
    phlex::experimental::product_specifications const& output() const override { return output_; }

    template <std::size_t... Is>
    auto call(function_t const& ft,
              messages_t<num_products> const& messages,
              std::index_sequence<Is...>,
              auto&&... resource_tokens)
    {
      if constexpr (num_products == 1ull) {
        return std::invoke(ft, std::get<Is>(input_).retrieve(messages)..., resource_tokens...);
      } else {
        return std::invoke(
          ft, std::get<Is>(input_).retrieve(std::get<Is>(messages))..., resource_tokens...);
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }

    input_retriever_types<input_product_types> input_{input_arguments<input_product_types>()};
    phlex::experimental::product_specifications output_;
    join_or_none_t<num_products> join_;
    node_t transform_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // PHLEX_CORE_TRANSFORM_NODE_HPP

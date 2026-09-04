#ifndef PHLEX_CORE_DECLARED_PREDICATE_HPP
#define PHLEX_CORE_DECLARED_PREDICATE_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/concepts.hpp"
#include "phlex/core/detail/filter_impl.hpp"
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
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace phlex::detail {

  class PHLEX_CORE_EXPORT declared_predicate : public products_consumer {
  public:
    declared_predicate(phlex::experimental::algorithm_name name,
                       std::vector<std::string> predicates,
                       product_selectors input_products);
    ~declared_predicate() override;

    virtual tbb::flow::sender<predicate_result>& sender() = 0;
  };

  using declared_predicate_ptr = std::unique_ptr<declared_predicate>;
  using declared_predicates = simple_ptr_map<declared_predicate_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits, typename... Resources>
  class predicate_node : public declared_predicate {
    using function_t = AlgorithmBits::bound_type;
    static constexpr auto num_resources = sizeof...(Resources);
    static constexpr auto num_products = AlgorithmBits::number_inputs - num_resources;
    using input_product_types = AlgorithmBits::template input_parameters<num_products>;
    using builder = node_builder<messages_t<num_products>,
                                 function_t,
                                 std::tuple<predicate_result>,
                                 std::tuple<Resources...>>;
    using node_t = builder::node_t;

  public:
    static constexpr auto number_output_products = 0ull;
    using node_ptr_type = declared_predicate_ptr;

    predicate_node(phlex::experimental::algorithm_name algo_name,
                   std::size_t concurrency,
                   std::vector<std::string> predicates,
                   tbb::flow::graph& g,
                   AlgorithmBits alg,
                   product_selectors input_products,
                   resource_catalog& resources) :
      declared_predicate{std::move(algo_name), std::move(predicates), std::move(input_products)},
      join_{make_join_or_none<num_products>(g, name().to_string(), layers())},
      predicate_{builder::make(
        g,
        concurrency,
        resources,
        alg.release_algorithm(),
        [this](function_t const& ft,
               messages_t<num_products> const& messages,
               auto&&... resource_tokens) -> predicate_result {
          auto const& msg = most_derived(messages);
          auto const& [store, message_id] = std::tie(msg.store, msg.id);

          bool const rc =
            call(ft, messages, std::make_index_sequence<num_products>{}, resource_tokens...);
          ++calls_;
          return {message_id, rc};
        })}
    {
      if constexpr (num_products > 1ull) {
        make_edge(join_, predicate_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_selector const& input_product) override
    {
      return receiver_for<num_products>(join_, input(), input_product, predicate_);
    }
    tbb::flow::sender<predicate_result>& sender() override
    {
      return builder::output_port(predicate_);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<num_products>(join_, predicate_);
    }

    template <std::size_t... Is>
    bool call(function_t const& ft,
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
    join_or_none_t<num_products> join_;
    node_t predicate_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // PHLEX_CORE_DECLARED_PREDICATE_HPP

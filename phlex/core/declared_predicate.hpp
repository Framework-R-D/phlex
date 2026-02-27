#ifndef PHLEX_CORE_DECLARED_PREDICATE_HPP
#define PHLEX_CORE_DECLARED_PREDICATE_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/detail/filter_impl.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/core/products_consumer.hpp"
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

namespace phlex::experimental {

  class declared_predicate : public products_consumer {
  public:
    declared_predicate(algorithm_name name,
                       std::vector<std::string> predicates,
                       product_queries input_products);
    virtual ~declared_predicate();

    virtual tbb::flow::sender<predicate_result>& sender() = 0;
  };

  using declared_predicate_ptr = std::unique_ptr<declared_predicate>;
  using declared_predicates = simple_ptr_map<declared_predicate_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class predicate_node : public declared_predicate {
    using InputArgs = typename AlgorithmBits::input_parameter_types;
    using function_t = typename AlgorithmBits::bound_type;
    static constexpr auto N = AlgorithmBits::number_inputs;

  public:
    static constexpr auto number_output_products = 0ull;
    using node_ptr_type = declared_predicate_ptr;

    predicate_node(algorithm_name name,
                   std::size_t concurrency,
                   std::vector<std::string> predicates,
                   tbb::flow::graph& g,
                   AlgorithmBits alg,
                   product_queries input_products) :
      declared_predicate{std::move(name), std::move(predicates), std::move(input_products)},
      join_{make_join_or_none<N>(g, full_name(), layers())},
      predicate_{
        g,
        concurrency,
        [this, ft = alg.release_algorithm()](messages_t<N> const& messages) -> predicate_result {
          auto const& msg = most_derived(messages);
          auto const& [store, message_id] = std::tie(msg.store, msg.id);

          bool const rc = call(ft, messages, std::make_index_sequence<N>{});
          ++calls_;
          return {message_id, rc};
        }}
    {
      if constexpr (N > 1ull) {
        make_edge(join_, predicate_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label, predicate_);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<N>(join_, predicate_);
    }
    tbb::flow::sender<predicate_result>& sender() override { return predicate_; }

    template <std::size_t... Is>
    bool call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      if constexpr (N == 1ull) {
        return std::invoke(ft, std::get<Is>(input_).retrieve(messages)...);
      } else {
        return std::invoke(ft, std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }

    input_retriever_types<InputArgs> input_{input_arguments<InputArgs>()};
    join_or_none_t<N> join_;
    tbb::flow::function_node<messages_t<N>, predicate_result> predicate_;
    std::atomic<std::size_t> calls_;
  };

}

#endif // PHLEX_CORE_DECLARED_PREDICATE_HPP

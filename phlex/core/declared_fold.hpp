#ifndef PHLEX_CORE_DECLARED_FOLD_HPP
#define PHLEX_CORE_DECLARED_FOLD_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/fold/send.hpp"
#include "phlex/core/fold_join_node.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT declared_fold : public products_consumer {
  public:
    declared_fold(phlex::experimental::algorithm_name name,
                  std::vector<std::string> predicates,
                  product_selectors input_products,
                  std::string partition_layer);
    ~declared_fold() override;

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual product_specifications const& output() const = 0;
    virtual tbb::flow::receiver<index_message>& partition_port() = 0;
    virtual std::size_t product_count() const = 0;
    phlex::experimental::identifier const& partition_layer() const { return partition_layer_; }

  private:
    phlex::experimental::identifier partition_layer_;
  };

  using declared_fold_ptr = std::unique_ptr<declared_fold>;
  using declared_folds = simple_ptr_map<declared_fold_ptr>;

  template <typename FoldResult, typename InitTuple, std::size_t... Is>
  auto make_initializer(InitTuple args, std::index_sequence<Is...>)
  {
    // The compiler emits a warning if an empty args tuple is captured by reference and then not expanded inside the lambda expression.  We therefore only capture args in the lambda expression if sizeof...(Is) > 0.
    if constexpr (sizeof...(Is) == 0) {
      return [](data_cell_index const&) { return std::make_unique<FoldResult>(); };
    } else {
      return [args](data_cell_index const&) {
        return std::make_unique<FoldResult>(std::get<Is>(args)...);
      };
    }
  }

  // =====================================================================================

  template <typename AlgorithmBits, typename InitTuple>
  class fold_node : public declared_fold {
    using all_parameter_types = typename AlgorithmBits::input_parameter_types;
    using result_type = std::decay_t<std::tuple_element_t<0, all_parameter_types>>;
    using input_parameter_types = skip_first_type<all_parameter_types>; // Skip fold object
    static constexpr auto num_inputs = std::tuple_size_v<input_parameter_types>;

    static constexpr std::size_t num_outputs = 1; // hard-coded for now
    using function_t = typename AlgorithmBits::bound_type;

  public:
    fold_node(phlex::experimental::algorithm_name algo_name,
              std::size_t concurrency,
              std::vector<std::string> predicates,
              tbb::flow::graph& g,
              AlgorithmBits alg,
              InitTuple initializer,
              product_selectors input_products,
              std::vector<std::string> output,
              std::string partition_layer) :
      declared_fold{std::move(algo_name),
                    std::move(predicates),
                    std::move(input_products),
                    std::move(partition_layer)},
      output_{to_product_specifications(name(), std::move(output), make_type_ids<result_type>())},
      join_{g,
            name().to_string(),
            this->partition_layer(),
            layers(),
            this->output(),
            make_initializer<result_type>(
              std::move(initializer), std::make_index_sequence<std::tuple_size_v<InitTuple>>{})},
      fold_{g,
            concurrency,
            [this, ft = alg.release_algorithm()](
              accumulator_with_messages<result_type, num_inputs> const& accum_with_msgs, auto&) {
              std::size_t const partition_hash = apply_fold(ft, accum_with_msgs);

              ++calls_;

              join_.notify_result_repeater_port().try_put(partition_hash);
            }}
    {
      make_edge(join_, fold_);
    }

  private:
    tbb::flow::receiver<message>& port_for(product_selector const& input_product) override
    {
      return receiver_for(join_, input(), input_product);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports(join_); }

    tbb::flow::receiver<index_message>& partition_port() override { return join_.partition_port(); }
    tbb::flow::sender<message>& output_port() override { return join_.output_port(); }
    product_specifications const& output() const override { return output_; }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return join_.emitted_result_count(); }

    std::size_t apply_fold(
      function_t const& ft,
      accumulator_with_messages<result_type, num_inputs> const& accum_with_msgs)
    {
      // We have to do awkward index management until we can use structured bindings with packs.
      auto& accumulator = std::get<0>(accum_with_msgs);
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        accumulator.partial_result->call(
          ft, std::get<Is>(input_).retrieve(std::get<Is + 1>(accum_with_msgs))...);
      }(std::make_index_sequence<num_inputs>{});
      return accumulator.index->hash();
    }

    input_retriever_types<input_parameter_types> input_{input_arguments<input_parameter_types>()};
    product_specifications output_;
    fold_join_node<result_type, num_inputs> join_;
    tbb::flow::multifunction_node<accumulator_with_messages<result_type, num_inputs>,
                                  message_tuple<1>>
      fold_;
    std::atomic<std::size_t> calls_;
  };
}

#endif // PHLEX_CORE_DECLARED_FOLD_HPP

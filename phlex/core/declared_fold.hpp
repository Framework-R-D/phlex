#ifndef PHLEX_CORE_DECLARED_FOLD_HPP
#define PHLEX_CORE_DECLARED_FOLD_HPP

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/fold/send.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/store_counters.hpp"
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

namespace phlex::experimental {
  class declared_fold : public products_consumer {
  public:
    declared_fold(algorithm_name name,
                  std::vector<std::string> predicates,
                  product_queries input_products);
    virtual ~declared_fold();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual tbb::flow::receiver<flush_message>& flush_port() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;
  };

  using declared_fold_ptr = std::unique_ptr<declared_fold>;
  using declared_folds = simple_ptr_map<declared_fold_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits, typename InitTuple>
  class fold_node : public declared_fold, private count_stores {
    using all_parameter_types = typename AlgorithmBits::input_parameter_types;
    using input_parameter_types = skip_first_type<all_parameter_types>; // Skip fold object
    static constexpr auto N = std::tuple_size_v<input_parameter_types>;
    using R = std::decay_t<std::tuple_element_t<0, all_parameter_types>>;

    static constexpr std::size_t M = 1; // hard-coded for now
    using function_t = typename AlgorithmBits::bound_type;

  public:
    fold_node(algorithm_name name,
              std::size_t concurrency,
              std::vector<std::string> predicates,
              tbb::flow::graph& g,
              AlgorithmBits alg,
              InitTuple initializer,
              product_queries product_labels,
              std::vector<std::string> output,
              std::string partition) :
      declared_fold{std::move(name), std::move(predicates), std::move(product_labels)},
      initializer_{std::move(initializer)},
      output_{to_product_specifications(full_name(), std::move(output), make_type_ids<R>())},
      partition_{std::move(partition)},
      flush_receiver_{g,
                      tbb::flow::unlimited,
                      [this](flush_message const& msg) -> tbb::flow::continue_msg {
                        auto const& [index, counts, original_message_id] = msg;
                        if (index->layer_name() != partition_) {
                          return {};
                        }

                        counter_for(index->hash()).set_flush_value(counts, original_message_id);
                        emit_and_evict_if_done(index);
                        return {};
                      }},
      join_{make_join_or_none<N>(
        g, full_name(), layers())}, // FIXME: This should change to include result product!
      fold_{
        g, concurrency, [this, ft = alg.release_algorithm()](messages_t<N> const& messages, auto&) {
          // N.B. The assumption is that a fold will *never* need to cache
          //      the product store it creates.  Any flush messages *do not* need
          //      to be propagated to downstream nodes.
          auto const& msg = most_derived(messages);
          auto const& index = msg.store->index();

          auto fold_index = index->parent(partition_);
          if (not fold_index) {
            return;
          }

          auto const& index_hash_for_counter = fold_index->hash();

          call(ft, messages, std::make_index_sequence<N>{});
          ++calls_;

          counter_for(index_hash_for_counter).increment(index->layer_hash());

          emit_and_evict_if_done(fold_index);
        }}
    {
      if constexpr (N > 1ull) {
        make_edge(join_, fold_);
      }
    }

  private:
    void emit_and_evict_if_done(data_cell_index_ptr const& fold_index)
    {
      if (auto counter = done_with(fold_index->hash())) {
        auto parent = std::make_shared<product_store>(fold_index, this->full_name());
        commit_(parent);
        ++product_count_;
        output_port<0>(fold_).try_put({parent, counter->original_message_id()});
      }
    }

    tbb::flow::receiver<message>& port_for(product_query const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label, fold_);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<N>(join_, fold_);
    }

    tbb::flow::receiver<flush_message>& flush_port() override { return flush_receiver_; }
    tbb::flow::sender<message>& sender() override { return output_port<0ull>(fold_); }
    tbb::flow::sender<message>& to_output() override { return sender(); }
    product_specifications const& output() const override { return output_; }

    template <std::size_t... Is>
    void call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      auto const parent_index = most_derived(messages).store->index()->parent(partition_);

      // FIXME: Not the safest approach!
      auto it = results_.find(parent_index->hash());
      if (it == results_.end()) {
        it =
          results_
            .insert({parent_index->hash(),
                     initialized_object(std::move(initializer_),
                                        std::make_index_sequence<std::tuple_size_v<InitTuple>>{})})
            .first;
      }

      if constexpr (N == 1ull) {
        std::invoke(ft, *it->second, std::get<Is>(input_).retrieve(messages)...);
      } else {
        std::invoke(ft, *it->second, std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    template <size_t... Is>
    auto initialized_object(InitTuple&& tuple, std::index_sequence<Is...>) const
    {
      return std::unique_ptr<R>{
        new R{std::forward<std::tuple_element_t<Is, InitTuple>>(std::get<Is>(tuple))...}};
    }

    auto commit_(product_store_ptr& store)
    {
      auto& result = results_.at(store->index()->hash());
      if constexpr (requires { send(*result); }) {
        store->add_product(output()[0].name(), send(*result));
      } else {
        store->add_product(output()[0].name(), std::move(result));
      }
      // Reclaim some memory; it would be better to erase the entire entry from the map,
      // but that is not thread-safe.
      result.reset();
    }

    InitTuple initializer_;
    input_retriever_types<input_parameter_types> input_{input_arguments<input_parameter_types>()};
    product_specifications output_;
    std::string partition_;
    tbb::flow::function_node<flush_message> flush_receiver_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, message_tuple<1>> fold_;
    tbb::concurrent_unordered_map<data_cell_index::hash_type, std::unique_ptr<R>> results_;
    std::atomic<std::size_t> calls_;
    std::atomic<std::size_t> product_count_;
  };
}

#endif // PHLEX_CORE_DECLARED_FOLD_HPP

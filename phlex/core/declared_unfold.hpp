#ifndef PHLEX_CORE_DECLARED_UNFOLD_HPP
#define PHLEX_CORE_DECLARED_UNFOLD_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace phlex::experimental {

  class PHLEX_CORE_EXPORT generator {
  public:
    explicit generator(product_store_const_ptr const& parent,
                       algorithm_name node_name,
                       std::string const& child_layer_name);

    std::size_t child_layer_hash() const { return child_layer_hash_; }
    std::size_t child_count() const { return child_counts_; }
    product_store_const_ptr make_child(std::size_t i, products new_products);

  private:
    product_store_ptr parent_;
    algorithm_name node_name_;
    // References declared_unfold::child_layer_, which outlives this short-lived object.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string const& child_layer_name_;
    std::size_t child_layer_hash_;
    std::size_t child_counts_ = 0;
  };

  class PHLEX_CORE_EXPORT declared_unfold : public products_consumer {
  public:
    declared_unfold(algorithm_name name,
                    std::vector<std::string> predicates,
                    product_queries input_products,
                    std::string child_layer);
    ~declared_unfold() override;

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual tbb::flow::sender<index_message>& output_index_port() = 0;
    virtual tbb::flow::sender<unfold_flush>& flush_sender() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;

    std::string const& child_layer() const noexcept { return child_layer_; }

  private:
    std::string child_layer_;
  };

  using declared_unfold_ptr = std::unique_ptr<declared_unfold>;
  using declared_unfolds = simple_ptr_map<declared_unfold_ptr>;

  // =====================================================================================

  template <typename Object, typename Predicate, typename Unfold>
  class unfold_node : public declared_unfold {
    using input_args = constructor_parameter_types<Object>;
    static constexpr std::size_t num_inputs = std::tuple_size_v<input_args>;
    static constexpr std::size_t num_outputs = number_output_objects<Unfold>;

  public:
    unfold_node(algorithm_name algo_name,
                std::size_t concurrency,
                std::vector<std::string> predicates,
                tbb::flow::graph& g,
                Predicate&& predicate,
                Unfold&& unfold,
                product_queries input_products,
                std::vector<std::string> output_product_suffixes,
                std::string child_layer_name) :
      declared_unfold{std::move(algo_name),
                      std::move(predicates),
                      std::move(input_products),
                      std::move(child_layer_name)},
      output_{to_product_specifications(name(),
                                        std::move(output_product_suffixes),
                                        make_type_ids<skip_first_type<return_type<Unfold>>>())},
      join_{make_join_or_none<num_inputs>(g, name().to_string(), layers())},
      unfold_{g,
              concurrency,
              [this, p = std::move(predicate), ufold = std::move(unfold)](
                messages_t<num_inputs> const& messages, auto& outputs) {
                auto const& msg = most_derived(messages);
                auto const& store = msg.store;

                generator gen{store, name(), child_layer()};
                call(
                  p, ufold, store->index(), gen, messages, std::make_index_sequence<num_inputs>{});
                std::get<2>(outputs).try_put({.index = store->index(),
                                              .layer_hash = gen.child_layer_hash(),
                                              .count = gen.child_count()});
              }}
    {
      if constexpr (num_inputs > 1ull) {
        make_edge(join_, unfold_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& input_product) override
    {
      return receiver_for<num_inputs>(join_, input(), input_product, unfold_);
    }
    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<num_inputs>(join_, unfold_);
    }

    tbb::flow::sender<message>& output_port() override
    {
      return tbb::flow::output_port<0>(unfold_);
    }
    tbb::flow::sender<index_message>& output_index_port() override
    {
      return tbb::flow::output_port<1>(unfold_);
    }
    tbb::flow::sender<unfold_flush>& flush_sender() override
    {
      return tbb::flow::output_port<2>(unfold_);
    }
    product_specifications const& output() const override { return output_; }

    template <std::size_t... Is>
    void call(Predicate const& predicate,
              Unfold const& unfold,
              data_cell_index_ptr const& unfolded_id,
              generator& g,
              messages_t<num_inputs> const& messages,
              std::index_sequence<Is...>)
    {
      ++calls_;
      Object obj = [this, &messages]() {
        if constexpr (num_inputs == 1ull) {
          return Object(std::get<Is>(input_).retrieve(messages)...);
        } else {
          return Object(std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
        }
      }();
      std::size_t counter = 0;
      auto running_value = obj.initial_value();
      while (std::invoke(predicate, obj, running_value)) {
        products new_products{num_outputs};
        auto new_id = unfolded_id->make_child(child_layer(), counter);
        if constexpr (requires { std::invoke(unfold, obj, running_value, *new_id); }) {
          auto [next_value, prods] = std::invoke(unfold, obj, running_value, *new_id);
          new_products.add_all(output_, std::move(prods));
          running_value = next_value;
        } else {
          auto [next_value, prods] = std::invoke(unfold, obj, running_value);
          new_products.add_all(output_, std::move(prods));
          running_value = next_value;
        }
        ++product_count_;

        auto child = g.make_child(counter++, std::move(new_products));
        auto const msg_id = msg_counter_.fetch_add(1);
        tbb::flow::output_port<0>(unfold_).try_put({.store = child, .id = msg_id});
        tbb::flow::output_port<1>(unfold_).try_put({.index = child->index(), .msg_id = msg_id});
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    input_retriever_types<input_args> input_{input_arguments<input_args>()};
    product_specifications output_;
    join_or_none_t<num_inputs> join_;
    tbb::flow::multifunction_node<messages_t<num_inputs>,
                                  std::tuple<message, index_message, unfold_flush>>
      unfold_;
    std::atomic<std::size_t> msg_counter_{}; // Is this sufficient?  Probably not.
    std::atomic<std::size_t> calls_{};
    std::atomic<std::size_t> product_count_{};
  };
}

#endif // PHLEX_CORE_DECLARED_UNFOLD_HPP

#ifndef PHLEX_CORE_DECLARED_UNFOLD_HPP
#define PHLEX_CORE_DECLARED_UNFOLD_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/multilayer_join_node.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
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

  class generator {
  public:
    explicit generator(product_store_const_ptr const& parent,
                       std::string node_name,
                       std::string const& child_layer_name);
    flush_counts_ptr flush_result() const;

    product_store_const_ptr make_child_for(std::size_t const data_cell_number,
                                           products new_products)
    {
      return make_child(data_cell_number, std::move(new_products));
    }

  private:
    product_store_const_ptr make_child(std::size_t i, products new_products);
    product_store_ptr parent_;
    std::string node_name_;
    std::string const& child_layer_name_;
    std::map<data_cell_index::hash_type, std::size_t> child_counts_;
  };

  class declared_unfold : public products_consumer {
  public:
    declared_unfold(algorithm_name name,
                    std::vector<std::string> predicates,
                    product_queries input_products,
                    std::string child_layer);
    virtual ~declared_unfold();

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual tbb::flow::sender<data_cell_index_ptr>& output_index_port() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;
    virtual flusher_t& flusher() = 0;

    std::string const& child_layer() const noexcept { return child_layer_; }

  private:
    std::string child_layer_;
  };

  using declared_unfold_ptr = std::unique_ptr<declared_unfold>;
  using declared_unfolds = simple_ptr_map<declared_unfold_ptr>;

  // =====================================================================================

  template <typename Object, typename Predicate, typename Unfold>
  class unfold_node : public declared_unfold {
    using InputArgs = constructor_parameter_types<Object>;
    static constexpr std::size_t N = std::tuple_size_v<InputArgs>;
    static constexpr std::size_t M = number_output_objects<Unfold>;

  public:
    unfold_node(algorithm_name name,
                std::size_t concurrency,
                std::vector<std::string> predicates,
                tbb::flow::graph& g,
                Predicate&& predicate,
                Unfold&& unfold,
                product_queries product_labels,
                std::vector<std::string> output_products,
                std::string child_layer_name) :
      declared_unfold{std::move(name),
                      std::move(predicates),
                      std::move(product_labels),
                      std::move(child_layer_name)},
      output_{to_product_specifications(full_name(),
                                        std::move(output_products),
                                        make_type_ids<skip_first_type<return_type<Unfold>>>())},
      join_{make_join_or_none<N>(g, full_name(), layers())},
      unfold_{g,
              concurrency,
              [this, p = std::move(predicate), ufold = std::move(unfold)](
                messages_t<N> const& messages, auto&) {
                auto const& msg = most_derived(messages);
                auto const& store = msg.store;

                std::size_t const original_message_id{msg_counter_};
                generator g{store, this->full_name(), child_layer()};
                call(p, ufold, store->index(), g, messages, std::make_index_sequence<N>{});

                flusher_.try_put({.index = store->index(),
                                  .counts = g.flush_result(),
                                  .original_id = original_message_id});
              }},
      flusher_{g}
    {
      if constexpr (N > 1ull) {
        make_edge(join_, unfold_);
      }
    }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label, unfold_);
    }
    std::vector<tbb::flow::receiver<message>*> ports() override
    {
      return input_ports<N>(join_, unfold_);
    }

    tbb::flow::sender<message>& output_port() override
    {
      return tbb::flow::output_port<0>(unfold_);
    }
    tbb::flow::sender<data_cell_index_ptr>& output_index_port() override
    {
      return tbb::flow::output_port<1>(unfold_);
    }
    product_specifications const& output() const override { return output_; }
    flusher_t& flusher() override { return flusher_; }

    template <std::size_t... Is>
    void call(Predicate const& predicate,
              Unfold const& unfold,
              data_cell_index_ptr const& unfolded_id,
              generator& g,
              messages_t<N> const& messages,
              std::index_sequence<Is...>)
    {
      ++calls_;
      Object obj = [this, &messages]() {
        if constexpr (N == 1ull) {
          return Object(std::get<Is>(input_).retrieve(messages)...);
        } else {
          return Object(std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
        }
      }();
      std::size_t counter = 0;
      auto running_value = obj.initial_value();
      while (std::invoke(predicate, obj, running_value)) {
        products new_products;
        auto new_id = unfolded_id->make_child(counter, child_layer());
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

        auto child = g.make_child_for(counter++, std::move(new_products));
        tbb::flow::output_port<0>(unfold_).try_put(
          {.store = child, .id = msg_counter_.fetch_add(1)});
        tbb::flow::output_port<1>(unfold_).try_put(child->index());
      }
    }

    named_index_ports index_ports() final { return join_.index_ports(); }
    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    input_retriever_types<InputArgs> input_{input_arguments<InputArgs>()};
    product_specifications output_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, std::tuple<message, data_cell_index_ptr>> unfold_;
    flusher_t flusher_;
    std::atomic<std::size_t> msg_counter_{}; // Is this sufficient?  Probably not.
    std::atomic<std::size_t> calls_{};
    std::atomic<std::size_t> product_count_{};
  };
}

#endif // PHLEX_CORE_DECLARED_UNFOLD_HPP

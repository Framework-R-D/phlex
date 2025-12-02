#ifndef PHLEX_CORE_DECLARED_UNFOLD_HPP
#define PHLEX_CORE_DECLARED_UNFOLD_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/end_of_message.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
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
                       std::string const& new_level_name);
    product_store_const_ptr flush_store() const;

    product_store_const_ptr make_child_for(std::size_t const level_number, products new_products)
    {
      return make_child(level_number, std::move(new_products));
    }

  private:
    product_store_const_ptr make_child(std::size_t i, products new_products);
    product_store_ptr parent_;
    std::string node_name_;
    std::string const& new_level_name_;
    std::map<level_id::hash_type, std::size_t> child_counts_;
  };

  class declared_unfold : public products_consumer {
  public:
    declared_unfold(algorithm_name name,
                    std::vector<std::string> predicates,
                    product_queries input_products);
    virtual ~declared_unfold();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;

  protected:
    using stores_t = tbb::concurrent_hash_map<level_id::hash_type, product_store_ptr>;
    using accessor = stores_t::accessor;
    using const_accessor = stores_t::const_accessor;

    void report_cached_stores(stores_t const& stores) const;
  };

  using declared_unfold_ptr = std::unique_ptr<declared_unfold>;
  using declared_unfolds = simple_ptr_map<declared_unfold_ptr>;

  // =====================================================================================

  template <typename Object, typename Predicate, typename Unfold>
  class unfold_node : public declared_unfold, private detect_flush_flag {
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
                std::string new_level_name) :
      declared_unfold{std::move(name), std::move(predicates), std::move(product_labels)},
      output_{to_product_specifications(full_name(),
                                        std::move(output_products),
                                        make_type_ids<skip_first_type<return_type<Unfold>>>())},
      new_level_name_{std::move(new_level_name)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      unfold_{
        g,
        concurrency,
        [this, p = std::move(predicate), ufold = std::move(unfold)](messages_t<N> const& messages,
                                                                    auto& output) {
          auto const& msg = most_derived(messages);
          auto const& store = msg.store;
          if (store->is_flush()) {
            flag_for(store->id()->hash()).flush_received(msg.id);
            std::get<0>(output).try_put(msg);
          } else if (accessor a; stores_.insert(a, store->id()->hash())) {
            std::size_t const original_message_id{msg_counter_};
            generator g{msg.store, this->full_name(), new_level_name_};
            call(p, ufold, msg.store->id(), g, msg.eom, messages, std::make_index_sequence<N>{});

            message const flush_msg{g.flush_store(), msg.eom, ++msg_counter_, original_message_id};
            std::get<0>(output).try_put(flush_msg);
            flag_for(store->id()->hash()).mark_as_processed();
          }

          if (done_with(store)) {
            stores_.erase(store->id()->hash());
          }
        }}
    {
      make_edge(join_, unfold_);
    }

    ~unfold_node() { report_cached_stores(stores_); }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label);
    }
    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& sender() override { return output_port<0>(unfold_); }
    tbb::flow::sender<message>& to_output() override { return sender(); }
    product_specifications const& output() const override { return output_; }

    template <std::size_t... Is>
    void call(Predicate const& predicate,
              Unfold const& unfold,
              level_id_ptr const& unfolded_id,
              generator& g,
              end_of_message_ptr const& eom,
              messages_t<N> const& messages,
              std::index_sequence<Is...>)
    {
      ++calls_;
      Object obj(std::get<Is>(input_).retrieve(messages)...);
      std::size_t counter = 0;
      auto running_value = obj.initial_value();
      while (std::invoke(predicate, obj, running_value)) {
        products new_products;
        auto new_id = unfolded_id->make_child(counter, new_level_name_);
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
        message const child_msg{child, eom->make_child(child->id()), ++msg_counter_};
        output_port<0>(unfold_).try_put(child_msg);
      }
    }

    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    input_retriever_types<InputArgs> input_{input_arguments<InputArgs>()};
    product_specifications output_;
    std::string new_level_name_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, messages_t<1u>> unfold_;
    tbb::concurrent_hash_map<level_id::hash_type, product_store_ptr> stores_;
    std::atomic<std::size_t> msg_counter_{}; // Is this sufficient?  Probably not.
    std::atomic<std::size_t> calls_{};
    std::atomic<std::size_t> product_count_{};
  };
}

#endif // PHLEX_CORE_DECLARED_UNFOLD_HPP

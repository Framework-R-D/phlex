#ifndef PHLEX_CORE_DECLARED_TRANSFORM_HPP
#define PHLEX_CORE_DECLARED_TRANSFORM_HPP

// FIXME: Add comments explaining the process.  For each implementation, explain what part
//        of the process a given section of code is addressing.

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
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

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace phlex::experimental {

  class declared_transform : public products_consumer {
  public:
    declared_transform(algorithm_name name,
                       std::vector<std::string> predicates,
                       product_queries input_products);
    virtual ~declared_transform();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;

  protected:
    using stores_t = tbb::concurrent_hash_map<data_cell_index::hash_type, product_store_ptr>;
    using accessor = stores_t::accessor;
    using const_accessor = stores_t::const_accessor;

    void report_cached_stores(stores_t const& stores) const;
  };

  using declared_transform_ptr = std::unique_ptr<declared_transform>;
  using declared_transforms = simple_ptr_map<declared_transform_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class transform_node : public declared_transform, private detect_flush_flag {
    using function_t = typename AlgorithmBits::bound_type;
    using input_parameter_types = typename AlgorithmBits::input_parameter_types;

    static constexpr auto N = AlgorithmBits::number_inputs;
    static constexpr auto M = number_output_objects<function_t>;

  public:
    using node_ptr_type = declared_transform_ptr;
    static constexpr auto number_output_products = M;

    transform_node(algorithm_name name,
                   std::size_t concurrency,
                   std::vector<std::string> predicates,
                   tbb::flow::graph& g,
                   AlgorithmBits alg,
                   product_queries input_products,
                   std::vector<std::string> output) :
      declared_transform{std::move(name), std::move(predicates), std::move(input_products)},
      output_{to_product_specifications(
        full_name(), std::move(output), make_output_type_ids<function_t>())},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      transform_{g,
                 concurrency,
                 [this, ft = alg.release_algorithm()](messages_t<N> const& messages, auto& output) {
                   auto const& msg = most_derived(messages);
                   auto const& [store, message_id] = std::tie(msg.store, msg.id);
                   auto& [stay_in_graph, to_output] = output;
                   if (store->is_flush()) {
                     receive_flush(msg);
                   } else {
                     accessor a;
                     if (stores_.insert(a, store->index()->hash())) {
                       auto result = call(ft, messages, std::make_index_sequence<N>{});
                       ++calls_;
                       ++product_count_[store->index()->layer_hash()];
                       products new_products;
                       new_products.add_all(output_, std::move(result));
                       a->second = std::make_shared<product_store>(
                         store->index(), this->full_name(), std::move(new_products));

                       message const new_msg{a->second, message_id};
                       stay_in_graph.try_put(new_msg);
                       to_output.try_put(new_msg);
                       flag_for(store->index()->hash()).mark_as_processed();
                     } else {
                       stay_in_graph.try_put({a->second, message_id});
                     }
                   }

                   if (done_with(store)) {
                     stores_.erase(store->index()->hash());
                   }
                 }}
    {
      make_edge(join_, transform_);
    }

    ~transform_node() { report_cached_stores(stores_); }

  private:
    tbb::flow::receiver<message>& port_for(product_query const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& sender() override { return output_port<0>(transform_); }
    tbb::flow::sender<message>& to_output() override { return output_port<1>(transform_); }
    product_specifications const& output() const override { return output_; }

    template <std::size_t... Is>
    auto call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      return std::invoke(ft, std::get<Is>(input_).retrieve(std::get<Is>(messages))...);
    }

    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final
    {
      std::size_t result{};
      for (auto const& count : product_count_ | std::views::values) {
        result += count.load();
      }
      return result;
    }

    input_retriever_types<input_parameter_types> input_{input_arguments<input_parameter_types>()};
    product_specifications output_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, messages_t<2u>> transform_;
    stores_t stores_;
    std::atomic<std::size_t> calls_;
    tbb::concurrent_unordered_map<std::size_t, std::atomic<std::size_t>> product_count_;
  };

}

#endif // PHLEX_CORE_DECLARED_TRANSFORM_HPP

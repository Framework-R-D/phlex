#ifndef phlex_core_declared_fold_hpp
#define phlex_core_declared_fold_hpp

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/fold/send.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/qualified_name.hpp"
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
                  specified_labels input_products);
    virtual ~declared_fold();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual qualified_names const& output() const = 0;
    virtual std::size_t product_count() const = 0;
  };

  using declared_fold_ptr = std::unique_ptr<declared_fold>;
  using declared_folds = simple_ptr_map<declared_fold_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class pre_fold {
    using all_parameter_types = typename AlgorithmBits::input_parameter_types;
    using input_parameter_types = skip_first_type<all_parameter_types>; // Skip fold object
    static constexpr auto N = std::tuple_size_v<input_parameter_types>;
    using R = std::decay_t<std::tuple_element_t<0, all_parameter_types>>;

    static constexpr std::size_t M = 1; // hard-coded for now
    using function_t = typename AlgorithmBits::bound_type;

    template <typename InitTuple>
    class total_fold;

  public:
    pre_fold(registrar<declared_fold_ptr> reg,
             algorithm_name name,
             std::size_t concurrency,
             std::vector<std::string> predicates,
             tbb::flow::graph& g,
             AlgorithmBits alg,
             specified_labels input_products) :
      name_{std::move(name)},
      concurrency_{concurrency},
      predicates_{std::move(predicates)},
      graph_{g},
      ft_{alg.release_algorithm()},
      product_labels_{std::move(input_products)},
      reg_{std::move(reg)}
    {
    }

    template <std::size_t Msize>
    auto& to(std::array<std::string, Msize> output_keys)
    {
      static_assert(
        M == Msize,
        "The number of function parameters is not the same as the number of returned output "
        "objects.");
      std::ranges::transform(output_keys, output_names_.begin(), to_qualified_name{name_});
      reg_.set_creator([this](auto) { return create(std::make_tuple()); });
      return *this;
    }

    auto& to(std::convertible_to<std::string> auto&&... ts)
    {
      static_assert(
        M == sizeof...(ts),
        "The number of function parameters is not the same as the number of returned output "
        "objects.");
      return to(std::array<std::string, M>{std::forward<decltype(ts)>(ts)...});
    }

    auto& partitioned_by(std::string const& level_name)
    {
      partition_ = level_name;
      return *this;
    }

    auto& initialized_with(auto&&... ts)
    {
      reg_.set_creator([this, init = std::tuple{ts...}](auto) { return create(std::move(init)); });
      return *this;
    }

  private:
    template <typename T>
    declared_fold_ptr create(T init)
    {
      if (empty(partition_)) {
        throw std::runtime_error("The fold range must be specified using the 'over(...)' syntax.");
      }
      return std::make_unique<total_fold<decltype(init)>>(std::move(name_),
                                                          concurrency_,
                                                          std::move(predicates_),
                                                          graph_,
                                                          std::move(ft_),
                                                          std::move(init),
                                                          std::move(product_labels_),
                                                          std::move(output_names_),
                                                          std::move(partition_));
    }

    algorithm_name name_;
    std::size_t concurrency_;
    std::vector<std::string> predicates_;
    tbb::flow::graph& graph_;
    function_t ft_;
    specified_labels product_labels_;
    std::string partition_{level_id::base().level_name()};
    std::array<qualified_name, M> output_names_;
    registrar<declared_fold_ptr> reg_;
  };

  template <typename AlgorithmBits>
  template <typename InitTuple>
  class pre_fold<AlgorithmBits>::total_fold : public declared_fold, private count_stores {
  public:
    total_fold(algorithm_name name,
               std::size_t concurrency,
               std::vector<std::string> predicates,
               tbb::flow::graph& g,
               function_t&& f,
               InitTuple initializer,
               specified_labels input_products,
               std::array<qualified_name, M> output,
               std::string partition) :
      declared_fold{std::move(name), std::move(predicates), std::move(input_products)},
      initializer_{std::move(initializer)},
      output_(output.begin(), output.end()),
      partition_{std::move(partition)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      fold_{
        g, concurrency, [this, ft = std::move(f)](messages_t<N> const& messages, auto& outputs) {
          // N.B. The assumption is that a fold will *never* need to cache
          //      the product store it creates.  Any flush messages *do not* need
          //      to be propagated to downstream nodes.
          auto const& msg = most_derived(messages);
          auto const& [store, original_message_id] = std::tie(msg.store, msg.original_id);

          if (not store->is_flush() and not store->id()->parent(partition_)) {
            return;
          }

          if (store->is_flush()) {
            // Downstream nodes always get the flush.
            get<0>(outputs).try_put(msg);
            if (store->id()->level_name() != partition_) {
              return;
            }
          }

          auto const& fold_store = store->is_flush() ? store : store->parent(partition_);
          assert(fold_store);
          auto const& id_hash_for_counter = fold_store->id()->hash();

          if (store->is_flush()) {
            counter_for(id_hash_for_counter).set_flush_value(store, original_message_id);
          } else {
            call(ft, messages, std::make_index_sequence<N>{});
            counter_for(id_hash_for_counter).increment(store->id()->level_hash());
          }

          if (auto counter = done_with(id_hash_for_counter)) {
            auto parent = fold_store->make_continuation(this->full_name());
            commit_(*parent);
            ++product_count_;
            // FIXME: This msg.eom value may be wrong!
            get<0>(outputs).try_put({parent, msg.eom, counter->original_message_id()});
          }
        }}
    {
      make_edge(join_, fold_);
    }

  private:
    tbb::flow::receiver<message>& port_for(specified_label const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& sender() override { return output_port<0ull>(fold_); }
    tbb::flow::sender<message>& to_output() override { return sender(); }
    qualified_names const& output() const override { return output_; }

    template <std::size_t... Is>
    void call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      auto const& parent_id = *most_derived(messages).store->id()->parent(partition_);
      // FIXME: Not the safest approach!
      auto it = results_.find(parent_id);
      if (it == results_.end()) {
        it =
          results_
            .insert({parent_id,
                     initialized_object(std::move(initializer_),
                                        std::make_index_sequence<std::tuple_size_v<InitTuple>>{})})
            .first;
      }
      ++calls_;
      return std::invoke(ft, *it->second, std::get<Is>(input_).retrieve(messages)...);
    }

    std::size_t num_calls() const final { return calls_.load(); }
    std::size_t product_count() const final { return product_count_.load(); }

    template <size_t... Is>
    auto initialized_object(InitTuple&& tuple, std::index_sequence<Is...>) const
    {
      return std::unique_ptr<R>{
        new R{std::forward<std::tuple_element_t<Is, InitTuple>>(std::get<Is>(tuple))...}};
    }

    void commit_(product_store& store)
    {
      auto& result = results_.at(*store.id());
      if constexpr (requires { send(*result); }) {
        store.add_product(output()[0].name(), send(*result));
      } else {
        store.add_product(output()[0].name(), std::move(*result));
      }
      // Reclaim some memory; it would be better to erase the entire entry from the map,
      // but that is not thread-safe.
      result.reset();
    }

    InitTuple initializer_;
    input_retriever_types<input_parameter_types> input_{input_arguments<input_parameter_types>()};
    qualified_names output_;
    std::string partition_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, messages_t<1>> fold_;
    tbb::concurrent_unordered_map<level_id, std::unique_ptr<R>> results_;
    std::atomic<std::size_t> calls_;
    std::atomic<std::size_t> product_count_;
  };
}

#endif // phlex_core_declared_fold_hpp

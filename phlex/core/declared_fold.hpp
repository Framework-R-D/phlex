#ifndef phlex_core_declared_fold_hpp
#define phlex_core_declared_fold_hpp

#include "phlex/concurrency.hpp"
#include "phlex/core/concepts.hpp"
#include "phlex/core/fold/send.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/node_options.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/qualified_name.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace phlex::experimental {
  class declared_fold : public products_consumer {
  public:
    declared_fold(algorithm_name name, std::vector<std::string> predicates);
    virtual ~declared_fold();

    virtual tbb::flow::sender<message>& sender() = 0;
    virtual tbb::flow::sender<message>& to_output() = 0;
    virtual qualified_names output() const = 0;
    virtual std::size_t product_count() const = 0;
  };

  using declared_fold_ptr = std::unique_ptr<declared_fold>;
  using declared_folds = std::map<std::string, declared_fold_ptr>;

  // Registering concrete folds

  template <is_fold_like FT, typename InputArgs>
  class pre_fold {
    using input_parameter_types = skip_first_type<function_parameter_types<FT>>; // Skip fold object
    static constexpr auto N = std::tuple_size_v<input_parameter_types>;
    using R = std::decay_t<std::tuple_element_t<0, function_parameter_types<FT>>>;

    static constexpr std::size_t M = 1; // hard-coded for now
    using function_t = FT;

    template <typename InitTuple>
    class total_fold;

  public:
    pre_fold(registrar<declared_fold_ptr> reg,
             algorithm_name name,
             std::size_t concurrency,
             std::vector<std::string> predicates,
             tbb::flow::graph& g,
             function_t&& f,
             std::array<specified_label, N> product_labels) :
      name_{std::move(name)},
      concurrency_{concurrency},
      predicates_{std::move(predicates)},
      graph_{g},
      ft_{std::move(f)},
      product_labels_{std::move(product_labels)},
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
      reg_.set([this] { return create(std::make_tuple()); });
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
      fold_interval_ = level_name;
      return *this;
    }

    auto& initialized_with(auto&&... ts)
    {
      reg_.set([this, init = std::tuple{ts...}] { return create(std::move(init)); });
      return *this;
    }

  private:
    template <typename T>
    declared_fold_ptr create(T init)
    {
      if (empty(fold_interval_)) {
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
                                                          std::move(fold_interval_));
    }

    algorithm_name name_;
    std::size_t concurrency_;
    std::vector<std::string> predicates_;
    tbb::flow::graph& graph_;
    function_t ft_;
    std::array<specified_label, N> product_labels_;
    std::string fold_interval_{level_id::base().level_name()};
    std::array<qualified_name, M> output_names_;
    registrar<declared_fold_ptr> reg_;
  };

  template <is_fold_like FT, typename InputArgs>
  template <typename InitTuple>
  class pre_fold<FT, InputArgs>::total_fold : public declared_fold, private count_stores {
  public:
    total_fold(algorithm_name name,
               std::size_t concurrency,
               std::vector<std::string> predicates,
               tbb::flow::graph& g,
               function_t&& f,
               InitTuple initializer,
               std::array<specified_label, N> product_labels,
               std::array<qualified_name, M> output,
               std::string fold_interval) :
      declared_fold{std::move(name), std::move(predicates)},
      initializer_{std::move(initializer)},
      product_labels_{std::move(product_labels)},
      input_{form_input_arguments<InputArgs>(full_name(), product_labels_)},
      output_{std::move(output)},
      fold_interval_{std::move(fold_interval)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      fold_{
        g, concurrency, [this, ft = std::move(f)](messages_t<N> const& messages, auto& outputs) {
          // N.B. The assumption is that a fold will *never* need to cache
          //      the product store it creates.  Any flush messages *do not* need
          //      to be propagated to downstream nodes.
          auto const& msg = most_derived(messages);
          auto const& [store, original_message_id] = std::tie(msg.store, msg.original_id);

          if (not store->is_flush() and not store->id()->parent(fold_interval_)) {
            return;
          }

          if (store->is_flush()) {
            // Downstream nodes always get the flush.
            get<0>(outputs).try_put(msg);
            if (store->id()->level_name() != fold_interval_) {
              return;
            }
          }

          auto const& fold_store = store->is_flush() ? store : store->parent(fold_interval_);
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
      return receiver_for<N>(join_, product_labels_, product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    tbb::flow::sender<message>& sender() override { return output_port<0ull>(fold_); }
    tbb::flow::sender<message>& to_output() override { return sender(); }
    specified_labels input() const override { return product_labels_; }
    qualified_names output() const override { return output_; }

    template <std::size_t... Is>
    void call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      auto const& parent_id = *most_derived(messages).store->id()->parent(fold_interval_);
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
    std::array<specified_label, N> product_labels_;
    input_retriever_types<InputArgs> input_;
    std::array<qualified_name, M> output_;
    std::string fold_interval_;
    join_or_none_t<N> join_;
    tbb::flow::multifunction_node<messages_t<N>, messages_t<1>> fold_;
    tbb::concurrent_unordered_map<level_id, std::unique_ptr<R>> results_;
    std::atomic<std::size_t> calls_;
    std::atomic<std::size_t> product_count_;
  };
}

#endif // phlex_core_declared_fold_hpp

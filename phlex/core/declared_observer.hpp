#ifndef PHLEX_CORE_DECLARED_OBSERVER_HPP
#define PHLEX_CORE_DECLARED_OBSERVER_HPP

#include "phlex/core/concepts.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/core/store_counters.hpp"
#include "phlex/metaprogramming/type_deduction.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace phlex::experimental {

  class declared_observer : public products_consumer {
  public:
    declared_observer(algorithm_name name,
                      std::vector<std::string> predicates,
                      specified_labels input_products);
    virtual ~declared_observer();

  protected:
    using hashes_t = tbb::concurrent_hash_map<level_id::hash_type, bool>;
    using accessor = hashes_t::accessor;

    void report_cached_hashes(hashes_t const& hashes) const;
  };

  using declared_observer_ptr = std::unique_ptr<declared_observer>;
  using declared_observers = simple_ptr_map<declared_observer_ptr>;

  // =====================================================================================

  template <typename AlgorithmBits>
  class observer_node : public declared_observer, private detect_flush_flag {
    using InputArgs = typename AlgorithmBits::input_parameter_types;
    using function_t = typename AlgorithmBits::bound_type;
    static constexpr auto N = AlgorithmBits::number_inputs;

  public:
    static constexpr auto number_output_products = 0;
    using node_ptr_type = declared_observer_ptr;

    observer_node(algorithm_name name,
                  std::size_t concurrency,
                  std::vector<std::string> predicates,
                  tbb::flow::graph& g,
                  AlgorithmBits alg,
                  specified_labels input_products) :
      declared_observer{std::move(name), std::move(predicates), std::move(input_products)},
      join_{make_join_or_none(g, std::make_index_sequence<N>{})},
      observer_{g,
                concurrency,
                [this, ft = alg.release_algorithm()](
                  messages_t<N> const& messages) -> oneapi::tbb::flow::continue_msg {
                  auto const& msg = most_derived(messages);
                  auto const& [store, message_id] = std::tie(msg.store, msg.id);
                  if (store->is_flush()) {
                    flag_for(store->id()->hash()).flush_received(message_id);
                  } else if (accessor a; needs_new(store, a)) {
                    call(ft, messages, std::make_index_sequence<N>{});
                    a->second = true;
                    flag_for(store->id()->hash()).mark_as_processed();
                  }

                  if (done_with(store)) {
                    cached_hashes_.erase(store->id()->hash());
                  }
                  return {};
                }}
    {
      make_edge(join_, observer_);
    }

    ~observer_node() { report_cached_hashes(cached_hashes_); }

  private:
    tbb::flow::receiver<message>& port_for(specified_label const& product_label) override
    {
      return receiver_for<N>(join_, input(), product_label);
    }

    std::vector<tbb::flow::receiver<message>*> ports() override { return input_ports<N>(join_); }

    bool needs_new(product_store_const_ptr const& store, accessor& a)
    {
      if (cached_hashes_.count(store->id()->hash()) > 0ull) {
        return false;
      }
      return cached_hashes_.insert(a, store->id()->hash());
    }

    template <std::size_t... Is>
    void call(function_t const& ft, messages_t<N> const& messages, std::index_sequence<Is...>)
    {
      ++calls_;
      return std::invoke(ft, std::get<Is>(input_).retrieve(messages)...);
    }

    std::size_t num_calls() const final { return calls_.load(); }

    input_retriever_types<InputArgs> input_{input_arguments<InputArgs>()};
    join_or_none_t<N> join_;
    tbb::flow::function_node<messages_t<N>> observer_;
    hashes_t cached_hashes_;
    std::atomic<std::size_t> calls_;
  };
}

#endif // PHLEX_CORE_DECLARED_OBSERVER_HPP

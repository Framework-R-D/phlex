#ifndef PHLEX_CORE_DETAIL_ACCUMULATOR_NODE_HPP
#define PHLEX_CORE_DETAIL_ACCUMULATOR_NODE_HPP

#include "phlex/core/fold/send.hpp"
#include "phlex/core/message.hpp"
#include "phlex/phlex_core_export.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_queue.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <string>

namespace phlex::experimental::detail {

  template <typename T>
  class accumulator {
  public:
    using sendable_t = sendable_type<T>;

    explicit accumulator(std::unique_ptr<T> initial_value) : accumulator_(std::move(initial_value))
    {
    }

    template <typename FT, typename... Args>
    void call(FT const& f, Args&&... args)
    {
      std::invoke(f, *accumulator_, args...);
    }

    auto release_as_product()
    {
      auto result = std::move(accumulator_);
      if constexpr (requires { send(*result); }) {
        return std::make_unique<product<sendable_t>>(send(*result));
      } else {
        return std::make_unique<product<sendable_t>>(std::move(*result));
      }
    }

  private:
    std::unique_ptr<T> accumulator_;
  };

  template <typename T>
  struct accumulator_message {
    phlex::data_cell_index_ptr index;
    std::shared_ptr<accumulator<T>> partial_result;
    std::size_t id;

    accumulator_message propagate_with_id(std::size_t new_id) const
    {
      return {.index = index, .partial_result = partial_result, .id = new_id};
    }

    message release_as_message(std::string const& node_name,
                               product_specifications const& output,
                               std::size_t original_id)
    {
      auto store = std::make_shared<product_store>(index, node_name);
      // FIXME: Only read the first output specification, which is a temporary limitation until
      // we support multiple outputs from folds.
      store->add_product(output.front(), partial_result->release_as_product());
      partial_result.reset();
      return {.store = std::move(store), .id = original_id};
    }
  };

  template <typename Result>
  struct accumulator_message_matcher {
    std::size_t operator()(accumulator_message<Result> const& msg) const { return msg.id; }
  };

  using accumulator_node_input =
    std::tuple<index_message, indexed_end_token, index_message, std::size_t>;

  template <typename Result>
  class PHLEX_CORE_EXPORT accumulator_node :
    public tbb::flow::composite_node<accumulator_node_input,
                                     std::tuple<message, accumulator_message<Result>>> {

    using result_initializer_t = std::function<std::unique_ptr<Result>(data_cell_index const&)>;

  public:
    accumulator_node(tbb::flow::graph& g,
                     std::string node_name,
                     identifier partition_layer_name,
                     product_specifications output,
                     result_initializer_t initializer);

    tbb::flow::receiver<index_message>& partition_port();
    tbb::flow::receiver<indexed_end_token>& flush_port();
    tbb::flow::receiver<index_message>& index_port();

    bool cache_is_empty() const;
    std::size_t cache_size() const;

    ~accumulator_node();

  private:
    using accumulator_msg_t = accumulator_message<Result>;
    using base_t =
      tbb::flow::composite_node<accumulator_node_input, std::tuple<message, accumulator_msg_t>>;
    using tagged_msg_t = tbb::flow::
      tagged_msg<std::size_t, index_message, indexed_end_token, index_message, std::size_t>;
    using multifunction_node_t =
      tbb::flow::multifunction_node<tagged_msg_t, std::tuple<message, accumulator_msg_t>>;

    struct cached_accumulator {
      std::shared_ptr<accumulator_msg_t> accumulator_msg;
      tbb::concurrent_queue<std::size_t> msg_ids{};
      std::atomic<int> counter;
      std::atomic_flag flush_received{};
      std::size_t original_message_id{};
    };

    using cache_t =
      tbb::concurrent_hash_map<std::size_t, cached_accumulator>; // Key is the index hash
    using accessor = cache_t::accessor;

    void emit_pending_ids(cached_accumulator* entry);
    void handle_partition_message(index_message const& msg);
    void handle_flush_token(indexed_end_token const& token);
    void handle_index_message(index_message const& msg);
    void cleanup_cache_entry(accessor& a);
    void increment_cache_entry_then_cleanup(std::size_t key);

    tbb::flow::indexer_node<index_message, indexed_end_token, index_message, std::size_t> indexer_;
    multifunction_node_t repeater_;
    cache_t cached_results_;
    std::string node_name_;
    identifier partition_layer_;
    result_initializer_t initializer_;
    product_specifications output_;
  };

  // ==============================================================================
  // Implementation

  template <typename Result>
  accumulator_node<Result>::accumulator_node(tbb::flow::graph& g,
                                             std::string node_name,
                                             identifier partition_layer_name,
                                             product_specifications output,
                                             result_initializer_t initializer) :
    base_t{g},
    indexer_{g},
    repeater_{g,
              tbb::flow::unlimited,
              [this](tagged_msg_t const& tagged, auto& /* outputs */) {
                if (tagged.tag() == 0) {
                  handle_partition_message(tagged.cast_to<index_message>());
                } else if (tagged.tag() == 1) {
                  handle_flush_token(tagged.cast_to<indexed_end_token>());
                } else if (tagged.tag() == 2) {
                  handle_index_message(tagged.cast_to<index_message>());
                } else {
                  assert(tagged.tag() == 3);
                  // This means that a fold operation has taken place, and an attempt should
                  // be made to emit the fold result and clean up the cache entry.
                  increment_cache_entry_then_cleanup(tagged.cast_to<std::size_t>());
                }
              }},
    node_name_{std::move(node_name)},
    partition_layer_{std::move(partition_layer_name)},
    initializer_{std::move(initializer)},
    output_{std::move(output)}
  {
    base_t::set_external_ports(
      typename base_t::input_ports_type{input_port<0>(indexer_),
                                        input_port<1>(indexer_),
                                        input_port<2>(indexer_),
                                        input_port<3>(indexer_)},
      typename base_t::output_ports_type{output_port<0>(repeater_), output_port<1>(repeater_)});
    make_edge(indexer_, repeater_);
  }

  template <typename Result>
  tbb::flow::receiver<index_message>& accumulator_node<Result>::partition_port()
  {
    return input_port<0>(indexer_);
  }

  template <typename Result>
  tbb::flow::receiver<indexed_end_token>& accumulator_node<Result>::flush_port()
  {
    return input_port<1>(indexer_);
  }

  template <typename Result>
  tbb::flow::receiver<index_message>& accumulator_node<Result>::index_port()
  {
    return input_port<2>(indexer_);
  }

  template <typename Result>
  bool accumulator_node<Result>::cache_is_empty() const
  {
    return cached_results_.empty();
  }

  template <typename Result>
  std::size_t accumulator_node<Result>::cache_size() const
  {
    return cached_results_.size();
  }

  template <typename Result>
  accumulator_node<Result>::~accumulator_node()
  {
    if (cached_results_.empty()) {
      return;
    }

    spdlog::warn(
      "[{}/{}] Cached accumulators: {}", node_name_, partition_layer_, cached_results_.size());
    for (auto const& [_, cache] : cached_results_) {
      if (cache.accumulator_msg) {
        spdlog::warn("[{}/{}]   Partition {}",
                     node_name_,
                     partition_layer_,
                     cache.accumulator_msg->index->to_string());
      } else {
        spdlog::warn("[{}/{}]   Partition index not yet received", node_name_, partition_layer_);
      }
    }
  }

  template <typename Result>
  void accumulator_node<Result>::emit_pending_ids(cached_accumulator* entry)
  {
    assert(entry->accumulator_msg);
    std::size_t msg_id{};
    while (entry->msg_ids.try_pop(msg_id)) {
      auto& accum_msg = entry->accumulator_msg;
      output_port<1>(repeater_).try_put(accum_msg->propagate_with_id(msg_id));
    }
  }

  template <typename Result>
  void accumulator_node<Result>::handle_partition_message(index_message const& msg)
  {
    auto const key = msg.index->hash();

    accessor a;
    cached_results_.insert(a, key);
    auto* entry = &a->second;
    entry->accumulator_msg.reset(new accumulator_msg_t{
      .index = msg.index,
      .partial_result = std::make_shared<accumulator<Result>>(initializer_(*msg.index))});
    entry->original_message_id = msg.msg_id;
    emit_pending_ids(entry);
  }

  template <typename Result>
  void accumulator_node<Result>::handle_flush_token(indexed_end_token const& token)
  {
    auto const& [index, count] = token;
    accessor a;
    cached_results_.insert(a, index->hash());
    auto* entry = &a->second;
    entry->counter -= count;
    std::ignore = entry->flush_received.test_and_set();
    cleanup_cache_entry(a);
  }

  template <typename Result>
  void accumulator_node<Result>::handle_index_message(index_message const& msg)
  {
    auto const& [index, msg_id, cache] = msg;
    assert(cache);
    auto const key = index->hash();

    accessor a;
    cached_results_.insert(a, key);
    auto* entry = &a->second;
    if (auto& accum_msg = entry->accumulator_msg) {
      output_port<1>(repeater_).try_put(accum_msg->propagate_with_id(msg_id));
      emit_pending_ids(entry);
    } else {
      entry->msg_ids.push(msg_id);
    }
  }

  template <typename Result>
  void accumulator_node<Result>::cleanup_cache_entry(accessor& a)
  {
    auto* entry = &a->second;
    if (entry->flush_received.test() and entry->counter == 0) {
      output_port<0>(repeater_).try_put(entry->accumulator_msg->release_as_message(
        node_name_, output_, entry->original_message_id));
      cached_results_.erase(a);
    }
  }

  template <typename Result>
  void accumulator_node<Result>::increment_cache_entry_then_cleanup(std::size_t key)
  {
    accessor a;
    if (!cached_results_.find(a, key)) {
      return;
    }
    ++a->second.counter;
    cleanup_cache_entry(a);
  }
}

#endif // PHLEX_CORE_DETAIL_ACCUMULATOR_NODE_HPP

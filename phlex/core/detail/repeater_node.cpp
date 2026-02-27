#include "phlex/core/detail/repeater_node.hpp"

#include "spdlog/spdlog.h"

#include <cassert>

namespace phlex::experimental::detail {

  repeater_node::repeater_node(tbb::flow::graph& g, std::string node_name, identifier layer_name) :
    base_t{g},
    indexer_{g},
    repeater_{g,
              tbb::flow::unlimited,
              [this](tagged_msg_t const& tagged, auto& /* outputs */) {
                std::size_t key = -1ull;
                if (tagged.is_a<message>()) {
                  key = handle_data_message(tagged.cast_to<message>());
                } else if (tagged.is_a<indexed_end_token>()) {
                  key = handle_flush_token(tagged.cast_to<indexed_end_token>());
                } else {
                  key = handle_index_message(tagged.cast_to<index_message>());
                }

                cleanup_cache_entry(key);
              }},
    node_name_{std::move(node_name)},
    layer_{std::move(layer_name)}
  {
    base_t::set_external_ports(base_t::input_ports_type{input_port<0>(indexer_),
                                                        input_port<1>(indexer_),
                                                        input_port<2>(indexer_)},
                               base_t::output_ports_type{output_port<0>(repeater_)});
    make_edge(indexer_, repeater_);
  }

  tbb::flow::receiver<message>& repeater_node::data_port() { return input_port<0>(indexer_); }

  tbb::flow::receiver<indexed_end_token>& repeater_node::flush_port()
  {
    return input_port<1>(indexer_);
  }

  tbb::flow::receiver<index_message>& repeater_node::index_port()
  {
    return input_port<2>(indexer_);
  }

  bool repeater_node::cache_is_empty() const { return cached_products_.empty(); }

  std::size_t repeater_node::cache_size() const { return cached_products_.size(); }

  repeater_node::~repeater_node()
  {
    if (cached_products_.empty()) {
      return;
    }

    spdlog::warn("[{}/{}] Cached messages: {}", node_name_, layer_, cached_products_.size());
    for (auto const& [_, cache] : cached_products_) {
      if (cache.data_msg) {
        spdlog::warn("[{}/{}]   Product for {}",
                     node_name_,
                     layer_,
                     cache.data_msg->store->index()->to_string());
      } else {
        spdlog::warn("[{}/{}]   Product not yet received", node_name_, layer_);
      }
    }
  }

  int repeater_node::emit_pending_ids(cached_product* entry)
  {
    assert(entry->data_msg);
    int num_emitted{};
    std::size_t msg_id{};
    while (entry->msg_ids.try_pop(msg_id)) {
      output_port<0>(repeater_).try_put({.store = entry->data_msg->store, .id = msg_id});
      ++num_emitted;
    }
    return num_emitted;
  }

  std::size_t repeater_node::handle_data_message(message const& msg)
  {
    auto const key = msg.store->index()->hash();

    // Pass-through mode; output directly without caching
    if (!cache_enabled_) {
      output_port<0>(repeater_).try_put(msg);
      return key;
    }

    // Caching mode; store product and drain any pending message IDs
    assert(msg.store);
    accessor a;
    cached_products_.insert(a, key);
    auto* entry = &a->second;
    entry->data_msg = std::make_shared<message>(msg);
    entry->counter += emit_pending_ids(entry);
    return key;
  }

  std::size_t repeater_node::handle_flush_token(indexed_end_token const& token)
  {
    auto const& [index, count] = token;
    auto const key = index->hash();
    accessor a;
    cached_products_.insert(a, key);
    auto* entry = &a->second;
    entry->counter -= count;
    std::ignore = entry->flush_received.test_and_set();
    return key;
  }

  std::size_t repeater_node::handle_index_message(index_message const& msg)
  {
    auto const& [index, msg_id, cache] = msg;
    auto const key = index->hash();

    // Caching already disabled; no action needed
    if (!cache_enabled_) {
      return key;
    }

    // Transition to pass-through mode; output any cached product and disable caching
    if (!cache) {
      cache_enabled_ = false;
      if (accessor a; cached_products_.find(a, key)) {
        auto* entry = &a->second;
        if (entry->data_msg) {
          output_port<0>(repeater_).try_put(*entry->data_msg);
          ++entry->counter;
        }
      }
      return key;
    }

    // Normal caching mode; either output cached product or queue message ID for later
    accessor a;
    cached_products_.insert(a, key);
    auto* entry = &a->second;
    if (entry->data_msg) {
      output_port<0>(repeater_).try_put({.store = entry->data_msg->store, .id = msg_id});
      entry->counter += 1 + emit_pending_ids(entry);
    } else {
      entry->msg_ids.push(msg_id);
    }
    return key;
  }

  void repeater_node::cleanup_cache_entry(std::size_t key)
  {
    accessor a;
    if (!cached_products_.find(a, key)) {
      return;
    }

    auto* entry = &a->second;
    if (!cache_enabled_) {
      if (entry->counter == 0) {
        assert(entry->data_msg);
        output_port<0>(repeater_).try_put(*entry->data_msg);
      }
      cached_products_.erase(a);
    } else if (entry->flush_received.test() and entry->counter == 0) {
      cached_products_.erase(a);
    }
  }

}

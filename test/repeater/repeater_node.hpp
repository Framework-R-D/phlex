#ifndef TEST_REPEATER_NODE_HPP
#define TEST_REPEATER_NODE_HPP

#include "message_types.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_queue.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <cassert>
#include <memory>
#include <string>

namespace phlex::test {

  using repeater_node_input = std::tuple<indexed_message, indexed_end_token, index_message>;

  class repeater_node :
    public tbb::flow::composite_node<repeater_node_input, indexed_message_tuple<1>> {
    using base_t = tbb::flow::composite_node<repeater_node_input, indexed_message_tuple<1>>;
    using tagged_msg_t =
      tbb::flow::tagged_msg<std::size_t, indexed_message, indexed_end_token, index_message>;
    using multifunction_node_t =
      tbb::flow::multifunction_node<tagged_msg_t, indexed_message_tuple<1>>;

    struct cached_product {
      std::shared_ptr<indexed_message> msg;
      tbb::concurrent_queue<std::size_t> msg_ids{};
      std::atomic<int> counter;
      std::atomic_flag flush_received{};
    };

    using cache_t = tbb::concurrent_hash_map<std::size_t, cached_product>;
    using accessor = cache_t::accessor;
    using const_accessor = cache_t::const_accessor;

  public:
    repeater_node(tbb::flow::graph& g) :
      base_t{g},
      indexer_{g},
      repeater_{g, tbb::flow::unlimited, [this](tagged_msg_t const& tagged, auto& outputs) {
                  auto& output = std::get<0>(outputs);

                  std::size_t key = -1ull;
                  if (tagged.is_a<indexed_message>()) {
                    key = handle_data_message(tagged.cast_to<indexed_message>(), output);
                  } else if (tagged.is_a<indexed_end_token>()) {
                    key = handle_flush_token(tagged.cast_to<indexed_end_token>());
                  } else {
                    key = handle_index_message(tagged.cast_to<index_message>(), output);
                  }

                  cleanup_cache_entry(key, output);
                }}
    {
      base_t::set_external_ports(base_t::input_ports_type{input_port<0>(indexer_),
                                                          input_port<1>(indexer_),
                                                          input_port<2>(indexer_)},
                                 base_t::output_ports_type{output_port<0>(repeater_)});
      make_edge(indexer_, repeater_);
    }

    tbb::flow::receiver<indexed_message>& data_port() { return input_port<0>(indexer_); }
    tbb::flow::receiver<indexed_end_token>& flush_port() { return input_port<1>(indexer_); }
    tbb::flow::receiver<index_message>& index_port() { return input_port<2>(indexer_); }

    ~repeater_node()
    {
      if (cached_products_.empty()) {
        return;
      }

      spdlog::warn("[{}/{}] Cached products {}", node_name_, layer_, cached_products_.size());
      for (auto const& [_, cache] : cached_products_) {
        if (cache.msg) {
          spdlog::warn(
            "[{}/{}]   Product for {}", node_name_, layer_, cache.msg->index->to_string());
        } else {
          spdlog::warn("[{}/{}]   Product for {}", node_name_, layer_, _);
        }
      }
    }

    void set_metadata(std::string node_name, std::string layer)
    {
      node_name_ = std::move(node_name);
      layer_ = std::move(layer);
    }

  private:
    using output_port_t = std::tuple_element_t<0, typename multifunction_node_t::output_ports_type>;

    int emit_pending_ids(data_cell_index_ptr const& index,
                         cached_product* entry,
                         output_port_t& output)
    {
      assert(entry->msg);
      int num_emitted{};
      std::size_t msg_id{};
      while (entry->msg_ids.try_pop(msg_id)) {
        output.try_put({.msg_id = msg_id, .index = index, .data = entry->msg->data});
        ++num_emitted;
      }
      return num_emitted;
    }

    std::size_t handle_data_message(indexed_message const& msg, output_port_t& output)
    {
      auto const key = msg.index->hash();

      // Pass-through mode; output directly without caching
      if (!cache_enabled_) {
        output.try_put(msg);
        return key;
      }

      // Caching mode; store product and drain any pending message IDs
      assert(msg.data);
      accessor a;
      cached_products_.insert(a, key);
      auto* entry = &a->second;
      entry->msg = std::make_shared<indexed_message>(msg);
      entry->counter += emit_pending_ids(msg.index, entry, output);
      return key;
    }

    std::size_t handle_flush_token(indexed_end_token const& token)
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

    std::size_t handle_index_message(index_message const& msg, output_port_t& output)
    {
      auto const& [msg_id, index, cache] = msg;
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
          if (entry->msg) {
            output.try_put(*entry->msg);
            ++entry->counter;
          }
        }
        return key;
      }

      // Normal caching mode; either output cached product or queue message ID for later
      accessor a;
      cached_products_.insert(a, key);
      auto* entry = &a->second;
      if (entry->msg) {
        output.try_put({.msg_id = msg_id, .index = index, .data = entry->msg->data});
        entry->counter += 1 + emit_pending_ids(index, entry, output);
      } else {
        entry->msg_ids.push(msg_id);
      }
      return key;
    }

    void cleanup_cache_entry(std::size_t key, output_port_t& output)
    {
      accessor a;
      if (!cached_products_.find(a, key)) {
        return;
      }

      auto* entry = &a->second;
      if (!cache_enabled_) {
        if (entry->counter == 0) {
          output.try_put(*entry->msg);
          ++entry->counter;
        }
        assert(entry->counter == 1);
        cached_products_.erase(a);
      } else if (entry->flush_received.test() and entry->counter == 0) {
        cached_products_.erase(a);
      }
    }

    tbb::flow::indexer_node<indexed_message, indexed_end_token, index_message> indexer_;
    multifunction_node_t repeater_;
    tbb::concurrent_hash_map<std::size_t, cached_product> cached_products_; // Key is the index hash
    std::atomic<bool> cache_enabled_{true};
    std::string node_name_;
    std::string layer_;
  };
}

#endif // TEST_REPEATER_NODE_HPP

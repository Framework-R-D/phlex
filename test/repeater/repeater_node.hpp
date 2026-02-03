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

  template <typename T>
  using repeater_node_input = std::tuple<indexed_message<T>, indexed_end_token, index_message>;

  template <typename T>
  class repeater_node :
    public tbb::flow::composite_node<repeater_node_input<T>, indexed_message_tuple<T>> {
    using base_t = tbb::flow::composite_node<repeater_node_input<T>, indexed_message_tuple<T>>;
    using tagged_repeater_msg =
      tbb::flow::tagged_msg<std::size_t, indexed_message<T>, indexed_end_token, index_message>;

    struct cached_product {
      std::shared_ptr<indexed_message<T>> msg_with_product;
      tbb::concurrent_queue<std::size_t> msg_ids{};
      std::atomic<int> counter;
      std::atomic_flag flush_received{};
    };

    using cached_products_t = tbb::concurrent_hash_map<std::size_t, cached_product>;
    using accessor = cached_products_t::accessor;
    using const_accessor = cached_products_t::const_accessor;

  public:
    repeater_node(tbb::flow::graph& g) :
      base_t{g},
      indexer_{g},
      repeater_{
        g, tbb::flow::unlimited, [this](tagged_repeater_msg const& tagged, auto& outputs) {
          // spdlog::warn("[{}/{}] Received {}", node_name_, layer_, tagged.tag());
          cached_product* entry{nullptr};
          data_cell_index_ptr current_index{};
          std::size_t key = -1ull;
          auto& output = std::get<0>(outputs);

          auto drain_cache = [&output](data_cell_index_ptr const& index,
                                       cached_product* entry,
                                       std::string const& node_name [[maybe_unused]],
                                       std::string const& layer [[maybe_unused]]) -> int {
            assert(entry->msg_with_product);
            int counter{};
            std::size_t new_msg_id{};
            while (entry->msg_ids.try_pop(new_msg_id)) {
              // spdlog::info("[{}/{}] => Outputting {} ({})",
              //              node_name,
              //              layer,
              //              index->to_string(),
              //              index->hash());
              output.try_put(
                {.msg_id = new_msg_id, .index = index, .data = entry->msg_with_product->data});
              ++counter;
            }
            return counter;
          };

          if (tagged.template is_a<indexed_message<T>>()) {
            auto const& msg = tagged.template cast_to<indexed_message<T>>();
            current_index = msg.index;
            key = msg.index->hash();
            if (!caching_required_) {
              spdlog::debug(
                "[{}/{}] -> Outputting {} ({})", node_name_, layer_, msg.index->to_string(), key);
              output.try_put(msg);
            } else {
              assert(msg.data);
              accessor a;
              cached_products_.insert(a, key);
              entry = &a->second;
              spdlog::debug("[{}/{}] Caching product for {} ({})",
                            node_name_,
                            layer_,
                            msg.index->to_string(),
                            key);
              entry->msg_with_product = std::make_shared<indexed_message<T>>(msg);
              entry->counter += drain_cache(msg.index, entry, node_name_, layer_);
            }
          } else if (tagged.template is_a<indexed_end_token>()) {
            auto const [index, count] = tagged.template cast_to<indexed_end_token>();
            spdlog::debug(
              "[{}/{}] Received flush {} ({})", node_name_, layer_, count, index->to_string());
            current_index = index;
            key = index->hash();
            accessor a;
            cached_products_.insert(a, key);
            entry = &a->second;
            entry->counter -= count;
            std::ignore = entry->flush_received.test_and_set();
          } else {

            auto const [msg_id, index, cache] = tagged.template cast_to<index_message>();
            key = index->hash();

            if (caching_required_ and !cache) {
              caching_required_ = false;
              // No caching needed
              accessor a;
              if (cached_products_.find(a, key)) {
                entry = &a->second;
                if (entry->msg_with_product) {
                  spdlog::debug(
                    "[{}/{}] +> Outputting {} ({})", node_name_, layer_, index->to_string(), key);
                  output.try_put(*entry->msg_with_product);
                  ++entry->counter;
                }
              }
            } else if (caching_required_) {
              // Caching required
              current_index = index;
              key = index->hash();
              accessor a;
              cached_products_.insert(a, key);
              entry = &a->second;
              if (entry->msg_with_product) {
                spdlog::debug(
                  "[{}/{}] <> Outputting {} ({})", node_name_, layer_, index->to_string(), key);
                output.try_put(
                  {.msg_id = msg_id, .index = index, .data = entry->msg_with_product->data});
                entry->counter += 1 + drain_cache(index, entry, node_name_, layer_);
              } else {
                entry->msg_ids.push(msg_id);
              }
            }
          }

          // Cleanup
          if (accessor a; cached_products_.find(a, key)) {
            entry = &a->second;
            if (!caching_required_) {
              if (entry->counter == 0) {
                spdlog::debug("[{}/{}] ++ Outputting {} ({})",
                              node_name_,
                              layer_,
                              entry->msg_with_product->index->to_string(),
                              key);
                output.try_put(*entry->msg_with_product);
                ++entry->counter;
              }
              assert(entry->counter == 1);
              cached_products_.erase(a);
            } else if (entry->flush_received.test() and entry->counter == 0) {
              spdlog::trace("Erasing cached product {}", key);
              cached_products_.erase(a);
            }
            entry = nullptr; // Do not attempt to use entry (stale!)
          }
        }}
    {
      base_t::set_external_ports(typename base_t::input_ports_type{input_port<0>(indexer_),
                                                                   input_port<1>(indexer_),
                                                                   input_port<2>(indexer_)},
                                 typename base_t::output_ports_type{output_port<0>(repeater_)});
      make_edge(indexer_, repeater_);
    }

    tbb::flow::receiver<indexed_message<T>>& data_port() { return input_port<0>(indexer_); }
    tbb::flow::receiver<indexed_end_token>& flush_port() { return input_port<1>(indexer_); }
    tbb::flow::receiver<index_message>& index_port() { return input_port<2>(indexer_); }

    ~repeater_node()
    {
      if (cached_products_.empty()) {
        return;
      }
      spdlog::warn("[{}/{}] Cached products {}", node_name_, layer_, cached_products_.size());
      for (auto const& [_, cache] : cached_products_) {
        if (cache.msg_with_product) {
          spdlog::warn("[{}/{}]   Product for {}",
                       node_name_,
                       layer_,
                       cache.msg_with_product->index->to_string());
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
    tbb::flow::indexer_node<indexed_message<T>, indexed_end_token, index_message> indexer_;
    tbb::flow::multifunction_node<tagged_repeater_msg, indexed_message_tuple<T>> repeater_;
    tbb::concurrent_hash_map<std::size_t, cached_product> cached_products_; // Key is the index hash
    std::atomic<unsigned> n_cached_products_{};
    std::atomic<bool> caching_required_{true};
    std::atomic<bool> erase_cache_{false};
    std::string node_name_;
    std::string layer_;
  };
}

#endif // TEST_REPEATER_NODE_HPP

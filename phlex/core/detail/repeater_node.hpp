#ifndef PHLEX_CORE_DETAIL_REPEATER_NODE_HPP
#define PHLEX_CORE_DETAIL_REPEATER_NODE_HPP

#include "phlex/core/message.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_queue.h"
#include "oneapi/tbb/flow_graph.h"

#include <atomic>
#include <memory>
#include <string>

namespace phlex::experimental::detail {

  using repeater_node_input = std::tuple<message, indexed_end_token, index_message>;

  class repeater_node : public tbb::flow::composite_node<repeater_node_input, message_tuple<1>> {
  public:
    repeater_node(tbb::flow::graph& g, std::string node_name, identifier layer_name);

    tbb::flow::receiver<message>& data_port();
    tbb::flow::receiver<indexed_end_token>& flush_port();
    tbb::flow::receiver<index_message>& index_port();

    bool cache_is_empty() const;
    std::size_t cache_size() const;

    ~repeater_node();

  private:
    using base_t = tbb::flow::composite_node<repeater_node_input, message_tuple<1>>;
    using tagged_msg_t =
      tbb::flow::tagged_msg<std::size_t, message, indexed_end_token, index_message>;
    using multifunction_node_t = tbb::flow::multifunction_node<tagged_msg_t, message_tuple<1>>;

    struct cached_product {
      std::shared_ptr<message> data_msg;
      tbb::concurrent_queue<std::size_t> msg_ids{};
      std::atomic<int> counter;
      std::atomic_flag flush_received{};
    };

    using cache_t = tbb::concurrent_hash_map<std::size_t, cached_product>; // Key is the index hash
    using accessor = cache_t::accessor;

    int emit_pending_ids(cached_product* entry);
    std::size_t handle_data_message(message const& msg);
    std::size_t handle_flush_token(indexed_end_token const& token);
    std::size_t handle_index_message(index_message const& msg);
    void cleanup_cache_entry(std::size_t key);

    tbb::flow::indexer_node<message, indexed_end_token, index_message> indexer_;
    multifunction_node_t repeater_;
    cache_t cached_products_;
    std::atomic<bool> cache_enabled_{true};
    std::string node_name_;
    identifier layer_;
  };
}

#endif // PHLEX_CORE_DETAIL_REPEATER_NODE_HPP

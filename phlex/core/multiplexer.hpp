#ifndef PHLEX_CORE_MULTIPLEXER_HPP
#define PHLEX_CORE_MULTIPLEXER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>

namespace phlex::experimental {
  namespace detail {
    using index_set_node = tbb::flow::broadcast_node<index_message>;
    using index_set_node_ptr = std::shared_ptr<index_set_node>;
    using flush_node = tbb::flow::broadcast_node<indexed_end_token>;

    class multilayer_slot {
    public:
      multilayer_slot(tbb::flow::graph& g,
                      std::string layer,
                      tbb::flow::receiver<indexed_end_token>* flush_port,
                      tbb::flow::receiver<index_message>* input_port);

      auto const& layer() const noexcept { return layer_; }
      void put_message(data_cell_index_ptr const& index, std::size_t message_id);
      void put_end_token(data_cell_index_ptr const& index);

      bool matches_exactly(std::string const& layer_path) const;
      bool is_parent_of(data_cell_index_ptr const& index) const;

    private:
      std::string layer_;
      detail::index_set_node broadcaster_;
      detail::flush_node flusher_;
      int counter_ = 0;
    };

    using multilayer_slots = std::vector<std::shared_ptr<multilayer_slot>>;
  }

  class layer_sentry {
  public:
    layer_sentry(flush_counters& counters,
                 flusher_t& flusher,
                 detail::multilayer_slots const& slots_for_layer,
                 data_cell_index_ptr index,
                 std::size_t message_id);
    ~layer_sentry();
    std::size_t depth() const;

  private:
    flush_counters& counters_;
    flusher_t& flusher_;
    detail::multilayer_slots const& slots_;
    data_cell_index_ptr index_;
    std::size_t message_id_;
  };

  class multiplexer {
  public:
    struct named_input_port {
      product_query product_label;
      tbb::flow::receiver<message>* port;
    };
    using named_input_ports_t = std::vector<named_input_port>;

    // map of node name to its input ports
    using head_ports_t = std::map<std::string, named_input_ports_t>;

    struct provider_input_port_t {
      product_query product_label;
      tbb::flow::receiver<index_message>* port;
    };
    using provider_input_ports_t = std::map<std::string, provider_input_port_t>;

    explicit multiplexer(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr index);

    void finalize(tbb::flow::graph& g,
                  provider_input_ports_t provider_input_ports,
                  std::map<std::string, named_index_ports> multilayers);
    void drain();
    flusher_t& flusher() { return flusher_; }

  private:
    void backout_to(data_cell_index_ptr store);
    void send_to_provider_index_nodes(data_cell_index_ptr const& index, std::size_t message_id);
    detail::multilayer_slots const& send_to_multilayer_join_nodes(data_cell_index_ptr const& index,
                                                                  std::size_t message_id);
    detail::index_set_node_ptr index_node_for(std::string const& layer);

    provider_input_ports_t provider_input_ports_;
    std::atomic<std::size_t> received_indices_{};
    flusher_t flusher_;
    flush_counters counters_;
    std::stack<layer_sentry> layers_;

    // ==========================================================================================
    // Routing to provider nodes
    // The following maps are used to route data-cell indices to provider nodes.
    // The first map is from layer name to the corresponding broadcaster node.
    std::unordered_map<std::string, detail::index_set_node_ptr> broadcasters_;
    // The second map is a cache from a layer hash matched to a broadcaster node, to avoid repeated
    // lookups for the same layer.
    std::unordered_map<std::size_t, detail::index_set_node_ptr> matched_broadcasters_;

    // ==========================================================================================
    // Routing to multi-layer join nodes
    // The first map is from the node name to the corresponding broadcaster nodes and flush nodes.
    std::unordered_map<std::string, detail::multilayer_slots> multibroadcasters_;
    // The second map is a cache from a layer hash matched to a set of multilayer slots, to avoid repeated lookups
    // for the same layer.
    std::unordered_map<std::size_t, detail::multilayer_slots> matched_routing_entries_;
    // The third map is a cache from a layer hash matched to a set of multilayer slots for the purposes of flushing,
    // to avoid repeated lookups for the same layer during flushing.
    std::unordered_map<std::size_t, detail::multilayer_slots> matched_flushing_entries_;
  };

}

#endif // PHLEX_CORE_MULTIPLEXER_HPP

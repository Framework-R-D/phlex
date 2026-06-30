#ifndef PHLEX_CORE_INDEX_ROUTER_HPP
#define PHLEX_CORE_INDEX_ROUTER_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_gate.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/identifier.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"
#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace phlex::experimental {
  namespace detail {
    using index_set_node = tbb::flow::broadcast_node<index_message>;
    using index_set_node_ptr = std::shared_ptr<index_set_node>;
    using flush_node = tbb::flow::broadcast_node<indexed_end_token>;

    // ==========================================================================================
    // A multilayer_slot manages routing and flushing for a single layer slot (a repeater) in
    // multi-layer join nodes. Each slot corresponds to one input data layer of a multi-layer
    // join operation.  It:
    //   (a) routes index messages to either the matching layer or its data-layer parent, and
    //   (b) emits flush tokens to the repeater to evict a cached data product from memory.
    class multilayer_slot;
    using multilayer_slots = std::vector<std::shared_ptr<multilayer_slot>>;
    using multilayer_slots_ptr = std::shared_ptr<multilayer_slots const>;
  }

  class PHLEX_CORE_EXPORT index_router {
  public:
    struct named_input_port {
      product_selector input_product;
      tbb::flow::receiver<message>* port{};
    };
    using named_input_ports_t = std::vector<named_input_port>;

    // map of node name to its input ports
    using head_ports_t = std::map<std::string, named_input_ports_t>;

    struct provider_input_port_t {
      product_selector input_product;
      tbb::flow::receiver<index_message>* port{};
    };
    using provider_input_ports_t = std::map<std::string, provider_input_port_t>;

    explicit index_router(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr const& index,
                              phlex::detail::index_flushes flushes);

    void establish_layers(std::vector<layer_path> const& layer_paths_from_driver,
                          std::vector<identifier> unfold_input_layer_names,
                          std::vector<identifier> unfold_output_layer_names);

    // Registers how many unfolds produce children from each input layer.  Must be called
    // before execution so that flush_gates are initialized with the correct expected
    // child count when they are first created.
    void register_unfold_count_per_input_layer(std::map<identifier, std::size_t> counts);

    void finalize(tbb::flow::graph& g,
                  provider_input_ports_t provider_input_ports,
                  std::map<std::string, named_index_ports> multilayer_join_ports);
    void drain(phlex::detail::index_flushes flushes);
    flusher_t& flusher() { return flusher_; }

    tbb::flow::function_node<index_message, data_cell_index_ptr>& unfold_index_receiver()
    {
      return unfold_index_receiver_;
    }
    tbb::flow::function_node<phlex::detail::unfold_flush>& unfold_flush_receiver()
    {
      return unfold_flush_receiver_;
    }

  private:
    data_cell_index_ptr route(data_cell_index_ptr const& index,
                              bool is_lowest_layer,
                              std::size_t message_id);
    bool index_is_lowest_layer(data_cell_index_ptr const& index);
    // Hash-only lookup, intended for classifying child layer hashes that arrive in flush
    // messages (where only the hash is available, not a data_cell_index).  Returns the
    // cached classification when known; defaults to lowest for unknown hashes, which is
    // correct for unfold outputs (the only source of unknown hashes) and consistent with
    // index_is_lowest_layer()'s fall-through default.
    bool is_lowest_layer_hash(std::size_t layer_hash) const;
    detail::index_set_node_ptr index_set_node_for(layer_path const& layer);
    detail::index_set_node_ptr index_set_node_for(data_cell_index_ptr const& index);
    std::pair<detail::multilayer_slots_ptr, detail::multilayer_slots_ptr> multilayer_slots_for(
      data_cell_index_ptr const& index);
    void update_flush_counts(phlex::detail::index_flushes flushes);
    void apply_expected_count(phlex::detail::flush_gate& gate,
                              data_cell_index::hash_type child_layer_hash,
                              std::size_t count);
    phlex::detail::flush_gate_ptr gate_for(data_cell_index_ptr const& index);
    void flush_if_done(data_cell_index_ptr index);

    tbb::flow::function_node<index_message, data_cell_index_ptr> unfold_index_receiver_;
    tbb::flow::function_node<phlex::detail::unfold_flush> unfold_flush_receiver_;
    std::atomic<std::size_t> received_indices_{};
    flusher_t flusher_;
    tbb::concurrent_unordered_map<std::size_t, bool> is_lowest_layer_hashes_;
    std::vector<identifier> unfold_input_layer_names_;
    std::vector<identifier> unfold_output_layer_names_;

    // ==========================================================================================
    // Routing to provider nodes
    // The following maps are used to route data-cell indices to provider nodes.
    // The first map is from layer name to the corresponding index-set node.
    tbb::concurrent_unordered_map<identifier, detail::index_set_node_ptr> index_set_nodes_;
    // The second map is a cache from a layer hash to an index-set node, to avoid
    // repeated lookups for the same layer.
    tbb::concurrent_unordered_map<std::size_t, detail::index_set_node_ptr> index_set_node_cache_;

    // ==========================================================================================
    // Routing to multi-layer join nodes
    // Maps from join-node name to the multilayer slots for that node.
    tbb::concurrent_unordered_map<identifier, detail::multilayer_slots> multilayer_join_slots_;

    // This struct lets get_multilayer_slots return message and end-token slots together,
    // instead of passing concurrent_hash_map accessors as output parameters.
    struct multilayer_slot_cache_entry {
      detail::multilayer_slots_ptr message_slots;
      detail::multilayer_slots_ptr end_token_slots;
    };
    // Cache from layer hash to matched message/end-token slots for that layer.
    using multilayer_slot_cache_t =
      tbb::concurrent_hash_map<std::size_t, multilayer_slot_cache_entry>;
    using multilayer_slot_cache_accessor = multilayer_slot_cache_t::accessor;
    using multilayer_slot_cache_const_accessor = multilayer_slot_cache_t::const_accessor;
    multilayer_slot_cache_t multilayer_slot_cache_;

    // ==========================================================================================
    // Flush gates (data-cell index hash is the key)
    using gates_t = tbb::concurrent_hash_map<std::size_t, phlex::detail::flush_gate_ptr>;
    using accessor = gates_t::accessor;
    using const_accessor = gates_t::const_accessor;
    gates_t flush_gates_;

    // Number of unfolds that will send flush messages for each input layer.  Used to
    // initialize flush_gates with the correct expected child count.
    std::map<identifier, std::size_t> unfold_count_per_input_layer_;
  };
}

#endif // PHLEX_CORE_INDEX_ROUTER_HPP

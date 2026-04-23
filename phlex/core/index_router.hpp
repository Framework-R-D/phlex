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

namespace phlex::detail {
  namespace internal {
    using index_set_node = tbb::flow::broadcast_node<index_message>;
    using index_set_node_ptr = std::shared_ptr<index_set_node>;

    // ==========================================================================================
    // A multilayer_slot (one per registered named_index_port) captures the routing decision
    // for one input slot of a multi-layer join node.  See the implementation file for the full
    // description.  The slot is concerned purely with message routing — flush-side metadata is
    // carried separately in a paired flush_spec (see below) so that the slot need not know
    // about counting layers or flush ports.
    class multilayer_slot;
    using multilayer_slots = std::vector<std::shared_ptr<multilayer_slot>>;
    using multilayer_slots_ptr = std::shared_ptr<multilayer_slots const>;

    // The flush-side metadata for one registered named_index_port, paired positionally
    // with the corresponding multilayer_slot in join_node_slots (below).  When the
    // paired slot exactly matches a routed partition index, the router materializes one
    // end_token_entry per descendant of the routed path whose trailing layer name equals
    // `counting_layer`; the entry's count is later drawn from the gate's
    // committed_counts_ at the resolved hash and forwarded to `flush_port`.
    struct flush_spec {
      phlex::experimental::identifier counting_layer;
      tbb::flow::receiver<indexed_end_token>* flush_port;
    };

    // Per multi-layer join node: the slots, and the flush metadata paired positionally
    // with each slot (slots[i] corresponds to flush_specs[i]).  Stored together so
    // that multilayer_slots_for can iterate the two in lockstep.
    struct join_node_slots {
      multilayer_slots slots;
      std::vector<flush_spec> flush_specs;
    };

    // An end_token_entry binds a downstream flush_port to a specific path-aware
    // committed-counts entry that the partition flush must subtract on the receiving side.
    // The router materializes one entry per `(slot, counting-layer descendant path)`
    // pair when resolving end tokens for a routed partition index.
    struct end_token_entry {
      std::size_t counting_layer_hash;
      tbb::flow::receiver<indexed_end_token>* flush_port;
    };
    using end_token_entries = std::vector<end_token_entry>;
    using end_token_entries_ptr = std::shared_ptr<end_token_entries const>;
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

    struct fold_input_port_t {
      phlex::experimental::identifier layer;
      tbb::flow::receiver<index_message>* partition_port;
    };
    using fold_partition_ports_t = std::map<std::string, fold_input_port_t>;

    // Pairs an unfold's input layer name with the child layer name it produces.  These
    // pairs let establish_layers() extend the static layer hierarchy with the dynamic
    // paths that unfolds introduce at runtime, so that downstream path-aware hash
    // resolution (e.g. counting_layer_hashes_under) can discover unfold-produced layers.
    struct unfold_layer_pair {
      phlex::experimental::identifier input;
      phlex::experimental::identifier output;
    };

    explicit index_router(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr const& index, index_flushes flushes);

    // Establishes the layer hierarchy, registers unfold metadata, and wires all TBB graph
    // edges needed before execution.
    //
    // `layer_paths_from_driver` supplies the static layer paths (e.g. /job, /job/event).
    // `unfold_layer_pairs` lets the router extend those paths with the dynamic layers
    // produced by unfolds.  For each pair (in, out) and each known path whose trailing
    // segment is `in`, a synthetic path with `out` appended is added to the hierarchy.
    // The augmentation is iterated to a fixed point so that chained unfolds (out of one
    // feeds into another) are fully resolved.
    // `unfold_count_per_input_layer` registers how many unfolds produce children from each
    // input layer, so that flush_gates are initialized with the correct expected child count
    // when they are first created.
    struct unfold_data {
      std::vector<unfold_layer_pair> layer_pairs;
      std::map<phlex::experimental::identifier, std::size_t> count_per_input_layer;
    };

    void finalize(tbb::flow::graph& g,
                  std::vector<layer_path> const& layer_paths_from_driver,
                  unfold_data unfolds,
                  provider_input_ports_t provider_input_ports,
                  fold_partition_ports_t fold_partition_ports,
                  std::map<std::string, named_index_ports> multilayer_join_ports);
    void drain(index_flushes flushes);

    tbb::flow::function_node<index_message, data_cell_index_ptr>& unfold_index_receiver()
    {
      return unfold_index_receiver_;
    }
    tbb::flow::function_node<unfold_flush>& unfold_flush_receiver()
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

    // finalize() helpers — each owns one initialization step.
    void establish_layer_hierarchy(
      std::vector<phlex::experimental::layer_path> layer_paths_from_driver,
      std::vector<unfold_layer_pair> const& layer_pairs);
    void wire_provider_index_sets(tbb::flow::graph& g, provider_input_ports_t provider_input_ports);
    void wire_fold_partition_index_sets(tbb::flow::graph& g,
                                        fold_partition_ports_t fold_partition_ports);
    void build_multilayer_join_slots(
      tbb::flow::graph& g, std::map<std::string, named_index_ports> multilayer_join_ports);
    internal::index_set_node_ptr index_set_node_for(phlex::experimental::layer_path const& layer);
    internal::index_set_node_ptr index_set_node_for(data_cell_index_ptr const& index);
    std::pair<internal::multilayer_slots_ptr, internal::end_token_entries_ptr> multilayer_slots_for(
      data_cell_index_ptr const& index);
    void update_flush_counts(index_flushes flushes);
    void apply_expected_count(flush_gate& gate,
                              data_cell_index::hash_type child_layer_hash,
                              std::size_t count);
    flush_gate_ptr gate_for(data_cell_index_ptr const& index);
    void flush_if_done(data_cell_index_ptr index);

    // Returns one path-aware layer_hash per static-hierarchy layer path whose trailing
    // segment equals `counting_layer_name` and whose path lies strictly under
    // `partition_layer_path`.  Used by `multilayer_slots_for` to materialize one
    // end_token_entry per descendant counting-layer path when a slot's counting layer
    // differs from its routing layer.
    std::vector<std::size_t> counting_layer_hashes_under(
      phlex::experimental::layer_path const& partition_layer_path,
      phlex::experimental::identifier const& counting_layer_name) const;

    // Returns the deepest (most-derived) layer name among `layer_names` according to the
    // static hierarchy in `sorted_layer_paths_`.  Depth is determined by the maximum path
    // length of any registered path whose trailing segment equals the layer name; the
    // name with the greatest such depth wins, with ties broken by lexicographic order on
    // the layer name itself for determinism.  Layer names that aren't present in
    // `sorted_layer_paths_` (rare — should only occur for genuinely-unknown layers)
    // contribute a depth of 0 and are therefore never selected when a known peer exists.
    phlex::experimental::identifier deepest_layer_name(
      std::vector<phlex::experimental::identifier> const& layer_names) const;

    tbb::flow::function_node<index_message, data_cell_index_ptr> unfold_index_receiver_;
    tbb::flow::function_node<unfold_flush> unfold_flush_receiver_;
    std::atomic<std::size_t> received_indices_{};
    tbb::concurrent_unordered_map<std::size_t, bool> is_lowest_layer_hashes_;
    // Layer paths from the driver, sorted lexicographically.  Used to resolve a slot's
    // counting-layer name into the set of path-aware layer hashes that the flush gate
    // will populate, when the counting layer differs from the routing layer.
    std::vector<phlex::experimental::layer_path> sorted_layer_paths_;

    // ==========================================================================================
    // Routing to provider nodes
    // The following maps are used to route data-cell indices to provider nodes.
    // The first map is from layer name to the corresponding index-set node.
    tbb::concurrent_unordered_map<phlex::experimental::identifier, internal::index_set_node_ptr>
      index_set_nodes_;
    // The second map is a cache from a layer hash to an index-set node, to avoid
    // repeated lookups for the same layer.
    tbb::concurrent_unordered_map<std::size_t, internal::index_set_node_ptr> index_set_node_cache_;

    // ==========================================================================================
    // Routing to multi-layer join nodes
    // Maps from join-node name to the multilayer slots for that node.
    tbb::concurrent_unordered_map<phlex::experimental::identifier, internal::join_node_slots>
      multilayer_join_slots_;

    // This struct lets multilayer_slots_for return message slots and end-token entries
    // together, instead of passing concurrent_hash_map accessors as output parameters.
    // The two are populated independently:
    //   - message_slots:    one entry per slot template whose routing layer covers this
    //                       routed index (either an exact match or a parent layer).
    //   - end_token_entries:one entry per (slot template, counting-layer descendant path).
    //                       For non-fold cases the counting layer equals the routing
    //                       layer, so a slot at the routed index's layer contributes a
    //                       single entry whose counting_layer_hash is the routed index's
    //                       own layer_hash.  For a fold's partition slot the routing layer
    //                       is the partition (e.g. "job") while the counting layer is the
    //                       fold's input data layer (e.g. "event"), so the slot may
    //                       contribute multiple entries — one per "event"-named
    //                       descendant of the partition path.
    struct multilayer_slot_cache_entry {
      internal::multilayer_slots_ptr message_slots;
      internal::end_token_entries_ptr end_token_entries;
    };
    // Cache from layer hash to matched message/end-token slots for that layer.
    using multilayer_slot_cache_t =
      tbb::concurrent_hash_map<std::size_t, multilayer_slot_cache_entry>;
    using multilayer_slot_cache_accessor = multilayer_slot_cache_t::accessor;
    using multilayer_slot_cache_const_accessor = multilayer_slot_cache_t::const_accessor;
    multilayer_slot_cache_t multilayer_slot_cache_;

    // ==========================================================================================
    // Flush gates (data-cell index hash is the key)
    using gates_t = tbb::concurrent_hash_map<std::size_t, flush_gate_ptr>;
    using accessor = gates_t::accessor;
    using const_accessor = gates_t::const_accessor;
    gates_t flush_gates_;

    // Number of unfolds that will send flush messages for each input layer.  Used to
    // initialize flush_gates with the correct expected child count.
    std::map<phlex::experimental::identifier, std::size_t> unfold_count_per_input_layer_;
  };
}

#endif // PHLEX_CORE_INDEX_ROUTER_HPP

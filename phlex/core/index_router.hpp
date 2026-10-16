#ifndef PHLEX_CORE_INDEX_ROUTER_HPP
#define PHLEX_CORE_INDEX_ROUTER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_gate.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/phlex_core_export.hpp"

#include <oneapi/tbb/concurrent_hash_map.h>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/flow_graph.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  namespace internal {
    using index_set_node = tbb::flow::broadcast_node<index_message>;
    using index_set_node_ptr = std::shared_ptr<index_set_node>;

    // ==========================================================================================
    // Each participant slot owns routing and its required non-owning completion receiver.
    class multilayer_slot;
    using multilayer_slots = std::vector<std::shared_ptr<multilayer_slot>>;
    using multilayer_slots_ptr = std::shared_ptr<multilayer_slots const>;

    // Resolve one invocation-path set from product inputs and every participant's ancestry.
    // Routing and completion counts both use this set, including unfold-generated paths.
    struct join_node_slots {
      multilayer_slots slots;
      std::vector<phlex::experimental::layer_path> invocation_paths;
    };

    // One completion token per slot combines all applicable descendant counting paths.
    // The hashes identify entries in the partition gate's committed_counts_; their sum balances
    // the receiving slot's pending invocations when the partition flushes.
    struct end_token_entry {
      std::vector<std::size_t> counting_layer_hashes;
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

    // Keeps an unfold's input layers together with the child layer it produces. Only the deepest
    // input on a compatible ancestor chain parents the children and receives the unfold's flush.
    // This metadata lets the router discover generated paths before any runtime indices arrive.
    struct unfold_layer_spec {
      std::string name;
      std::vector<phlex::experimental::identifier> input_layers;
      phlex::experimental::identifier output_layer;
    };

    explicit index_router(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr const& index, index_flushes const& flushes);

    // One entry per uniquely named unfold node, as guaranteed by framework registration.
    using unfold_data = std::vector<unfold_layer_spec>;

    // Establishes the layer hierarchy, registers unfold flush expectations, and wires the TBB
    // graph edges needed before execution. Driver declarations supply the static paths; their
    // intermediate parents and implicit /job roots are included in the hierarchy.
    // Unfold-generated paths and per-parent flush expectations are resolved together to a fixed
    // point so chained unfolds are handled regardless of registration order. Each parent gate
    // then waits for every unfold that produces children from that particular hierarchy path.
    void finalize(tbb::flow::graph& g,
                  std::vector<phlex::experimental::layer_path> const& layer_paths_from_driver,
                  unfold_data const& unfolds,
                  provider_input_ports_t provider_input_ports,
                  fold_partition_ports_t fold_partition_ports,
                  std::map<std::string, join_registration> const& multilayer_join_ports);
    void drain(index_flushes const& flushes);

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
    bool index_is_lowest_layer(data_cell_index_ptr const& index) const;
    // Hash-only lookup for child paths from flush messages, where no data_cell_index is available.
    // Returns the cached classification for known paths; unknown hashes return true.
    bool is_lowest_layer_hash(std::size_t layer_hash) const;

    // finalize() helpers — each owns one initialization step.
    void establish_layer_hierarchy(
      std::vector<phlex::experimental::layer_path> const& layer_paths_from_driver,
      unfold_data const& unfolds);
    void wire_provider_index_sets(tbb::flow::graph& g, provider_input_ports_t provider_input_ports);
    void wire_fold_partition_index_sets(tbb::flow::graph& g,
                                        fold_partition_ports_t fold_partition_ports);
    void build_multilayer_join_slots(
      tbb::flow::graph& g, std::map<std::string, join_registration> const& multilayer_join_ports);
    internal::index_set_node_ptr index_set_node_for(phlex::experimental::layer_path const& layer);
    internal::index_set_node_ptr index_set_node_for(data_cell_index_ptr const& index);
    std::pair<internal::multilayer_slots_ptr, internal::end_token_entries_ptr> multilayer_slots_for(
      data_cell_index_ptr const& index);
    void update_flush_counts(index_flushes const& flushes);
    void apply_expected_count(flush_gate& gate,
                              data_cell_index::hash_type child_layer_hash,
                              std::size_t count);
    flush_gate_ptr gate_for(data_cell_index_ptr const& index);
    void flush_if_done(data_cell_index_ptr index);

    tbb::flow::function_node<index_message, data_cell_index_ptr> unfold_index_receiver_;
    tbb::flow::function_node<unfold_flush> unfold_flush_receiver_;
    std::atomic<std::size_t> received_indices_;
    tbb::concurrent_unordered_map<std::size_t, bool> is_lowest_layer_hashes_;
    // Driver and unfold paths, sorted lexicographically, used to resolve invocation domains.
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

    // This struct lets multilayer_slots_for return message slots and end-token entries together,
    // instead of passing concurrent_hash_map accessors as output parameters. End-token entries
    // contain one combined descendant count per receiving slot.
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

    // Layer-path hashes, not bare names: repeated names can have different unfold parents.
    // These counts are passed to flush_gate; see its expected_flush_count documentation for why
    // a gate may need to wait for flushes from multiple unfolds.
    std::map<std::size_t, std::size_t> number_unfolds_per_parent_path_;
  };
}

#endif // PHLEX_CORE_INDEX_ROUTER_HPP

#include "phlex/core/index_router.hpp"

#include "phlex/model/flush_gate.hpp"
#include "phlex/utilities/bulleted_list.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <iterator>
#include <ranges>
#include <stdexcept>

using namespace phlex::experimental;

namespace phlex::experimental {

  //========================================================================================
  // multilayer_slot implementation
  //
  // A multilayer_slot is a static, pre-registered routing component created once per
  // `named_index_port` in `finalize()`.  Each slot is responsible *only for message routing*: it
  // decides whether a routed index covers this slot (via `matches_exactly` / `is_parent_of`) and
  // forwards index messages to a single downstream `input_port` at the appropriate layer.
  //
  // The flush-side metadata for the same `named_index_port` — namely the `counting_layer` name
  // used to look up `committed_counts_` entries at flush time, and the downstream `flush_port`
  // that receives `indexed_end_token`s — lives in a paired `flush_spec` (see below).  The two are
  // stored side-by-side in `join_node_slots` so that `multilayer_slots_for` can pair them up
  // while resolving end-token entries for a routed partition index.
  namespace detail {
    class multilayer_slot {
    public:
      multilayer_slot(tbb::flow::graph& g,
                      identifier layer,
                      tbb::flow::receiver<index_message>* input_port);

      void put_message(data_cell_index_ptr const& index, std::size_t message_id);

      bool matches_exactly(layer_path const& layer_path) const;
      bool is_parent_of(data_cell_index_ptr const& index) const;

      identifier const& layer() const { return layer_; }

    private:
      identifier layer_;
      index_set_node broadcaster_;
    };

    multilayer_slot::multilayer_slot(tbb::flow::graph& g,
                                     identifier layer,
                                     tbb::flow::receiver<index_message>* input_port) :
      layer_{std::move(layer)}, broadcaster_{g}
    {
      make_edge(broadcaster_, *input_port);
    }

    void multilayer_slot::put_message(data_cell_index_ptr const& index, std::size_t message_id)
    {
      if (layer_ == index->layer_name()) {
        broadcaster_.try_put({.index = index, .msg_id = message_id, .cache = false});
        return;
      }

      broadcaster_.try_put({.index = index->parent(layer_), .msg_id = message_id});
    }

    bool multilayer_slot::matches_exactly(layer_path const& layer_path) const
    {
      return layer_path.ends_with(layer_);
    }

    bool multilayer_slot::is_parent_of(data_cell_index_ptr const& index) const
    {
      return index->parent(layer_) != nullptr;
    }
  }

  //========================================================================================
  // index_router implementation
  index_router::index_router(tbb::flow::graph& g) :
    unfold_index_receiver_{g,
                           tbb::flow::unlimited,
                           [this](index_message const& msg) -> data_cell_index_ptr {
                             auto const& [index, message_id, _] = msg;
                             assert(index);
                             return route(index, index_is_lowest_layer(index), message_id);
                           }},
    unfold_flush_receiver_{
      g, tbb::flow::unlimited, [this](unfold_flush input) -> tbb::flow::continue_msg {
        auto&& [index, layer_hash, count] = input;
        apply_expected_count(*gate_for(index), layer_hash, count);
        flush_if_done(index);
        return {};
      }}
  {
  }

  void index_router::finalize(tbb::flow::graph& g,
                              std::vector<layer_path> const& layer_paths_from_driver,
                              unfold_data unfolds,
                              provider_input_ports_t provider_input_ports,
                              fold_partition_ports_t fold_partition_ports,
                              std::map<std::string, named_index_ports> multilayer_join_ports)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());

    establish_layer_hierarchy(std::move(layer_paths_from_driver), unfolds.layer_pairs);
    unfold_count_per_input_layer_ = std::move(unfolds.count_per_input_layer);
    wire_provider_index_sets(g, std::move(provider_input_ports));
    wire_fold_partition_index_sets(g, std::move(fold_partition_ports));
    build_multilayer_join_slots(g, std::move(multilayer_join_ports));
  }

  // --------------------------------------------------------------------------------------------
  // Extend the static layer hierarchy with the dynamic paths that unfolds will produce at runtime.
  // For each pair (in, out) and each known path P whose trailing segment is `in`, add a path
  // `P + [out]`.  Iterate to a fixed point so that chained unfolds (the output of one feeding the
  // input of another) are fully expanded.  After sorting, populate is_lowest_layer_hashes_ for
  // every path so that index_is_lowest_layer() can give a definitive answer without a heuristic
  // fallback.
  void index_router::establish_layer_hierarchy(std::vector<layer_path> layer_paths_from_driver,
                                               std::vector<unfold_layer_pair> const& layer_pairs)
  {
    sorted_layer_paths_ = std::move(layer_paths_from_driver);
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto const& [input_layer, output_layer] : layer_pairs) {
        std::string_view const output_name = static_cast<std::string_view>(output_layer);
        // Snapshot current size: new paths appended in this loop become candidates only in the next
        // fixed-point iteration.  This keeps the inner loop deterministic and avoids unbounded
        // growth from self-referential pairs (which shouldn't occur but would otherwise loop
        // forever).
        std::size_t const snapshot = sorted_layer_paths_.size();
        for (std::size_t i = 0; i < snapshot; ++i) {
          auto const& parent_path = sorted_layer_paths_[i];
          if (not parent_path.ends_with(input_layer)) {
            continue;
          }
          layer_path candidate{parent_path.to_string() + "/" + std::string(output_name)};
          if (not std::ranges::contains(sorted_layer_paths_, candidate)) {
            sorted_layer_paths_.push_back(std::move(candidate));
            changed = true;
          }
        }
      }
    }

    std::ranges::sort(sorted_layer_paths_);

    // In sorted order, a path can only be a prefix of paths that follow it.
    for (std::size_t i = 0; i < sorted_layer_paths_.size(); ++i) {
      auto const layer_hash = sorted_layer_paths_[i].hash();
      bool const is_lowest_layer =
        i + 1 == sorted_layer_paths_.size() or
        not sorted_layer_paths_[i].is_strict_prefix_of(sorted_layer_paths_[i + 1]);
      // Record every known layer, both lowest and non-lowest.  Pre-populating the lowest entries
      // lets index_is_lowest_layer() return a definitive answer without falling back to the
      // unfold-name-based heuristic, which is important now that the augmentation above already
      // accounts for unfold-produced layers.
      is_lowest_layer_hashes_.emplace(layer_hash, is_lowest_layer);
    }
  }

  void index_router::wire_provider_index_sets(tbb::flow::graph& g,
                                              provider_input_ports_t provider_input_ports)
  {
    for (auto& [input_product, provider_port] : provider_input_ports | std::views::values) {
      auto [it, _] =
        index_set_nodes_.emplace(input_product.layer, std::make_shared<detail::index_set_node>(g));
      make_edge(*it->second, *provider_port);
    }
  }

  void index_router::wire_fold_partition_index_sets(tbb::flow::graph& g,
                                                    fold_partition_ports_t fold_partition_ports)
  {
    for (auto& [fold_node_name, partition_port] : fold_partition_ports) {
      auto const& [layer, port] = partition_port;
      auto [it, _] = index_set_nodes_.emplace(static_cast<identifier const&>(layer),
                                              std::make_shared<detail::index_set_node>(g));
      make_edge(*it->second, *port);
    }
  }

  // --------------------------------------------------------------------------------------------
  // For each multi-layer join node: compute its deepest slot layer, normalize any slot whose
  // counting_layer was left equal to its routing layer to the node's deepest layer instead,
  // construct the multilayer_slot objects, and store everything in multilayer_join_slots_.
  void index_router::build_multilayer_join_slots(
    tbb::flow::graph& g, std::map<std::string, named_index_ports> multilayer_join_ports)
  {
    for (auto const& [node_name, join_ports] : multilayer_join_ports) {
      spdlog::trace("Making multilayer slots for {}", node_name);

      // Compute the node's deepest slot layer.  The slot whose routing layer equals the deepest is
      // the one that drives the join's tag stream: every other slot in the node will be triggered
      // (and have its counter incremented) once per cell at that deepest layer.  A slot's flush
      // count must therefore balance against the deepest layer's child count under the routed
      // partition path, not against its own routing layer.  We normalize here so that join-node
      // authors don't have to reason about depth: any slot that left `counting_layer == layer`
      // (the "use my routing layer" default) gets the node's deepest layer instead.  Slots that
      // explicitly chose a different counting layer (e.g. fold_join_node's partition slot, which
      // selects the fold's data input layer) are left untouched.
      std::vector<identifier> slot_layers;
      slot_layers.reserve(join_ports.size());
      for (auto const& port : join_ports) {
        slot_layers.push_back(port.layer);
      }
      identifier const node_deepest_layer = deepest_layer_name(slot_layers);

      detail::join_node_slots node_slots;
      node_slots.slots.reserve(join_ports.size());
      node_slots.flush_specs.reserve(join_ports.size());
      for (auto const& [layer, counting_layer, flush_port, input_port] : join_ports) {
        identifier const effective_counting_layer =
          (counting_layer == layer) ? node_deepest_layer : counting_layer;
        node_slots.slots.push_back(std::make_shared<detail::multilayer_slot>(g, layer, input_port));
        node_slots.flush_specs.push_back(
          {.counting_layer = effective_counting_layer, .flush_port = flush_port});
      }
      multilayer_join_slots_.emplace(identifier{node_name}, std::move(node_slots));
    }
  }

  data_cell_index_ptr index_router::route(data_cell_index_ptr const& index, index_flushes flushes)
  {
    update_flush_counts(std::move(flushes));
    return route(index, index_is_lowest_layer(index), received_indices_.fetch_add(1));
  }

  data_cell_index_ptr index_router::route(data_cell_index_ptr const& index,
                                          bool const is_lowest_layer,
                                          std::size_t const message_id)
  {
    if (auto index_set_node = index_set_node_for(index)) {
      index_set_node->try_put({.index = index, .msg_id = message_id});
    }

    auto [message_slots, end_token_entries] = multilayer_slots_for(index);
    for (auto const& slot : *message_slots) {
      slot->put_message(index, message_id);
    }

    // Lowest-layer indices have no flush gate and contribute to their parent's readiness solely
    // through the expected-count message that announced them — nothing to do here for them.
    if (is_lowest_layer) {
      return index;
    }

    gate_for(index)->set_flush_callback(
      [end_token_entries = std::move(end_token_entries)](flush_gate const& fc) {
        for (auto const& entry : *end_token_entries) {
          auto const count = fc.committed_count_for_layer(entry.counting_layer_hash);
          entry.flush_port->try_put({.index = fc.index(), .count = static_cast<int>(count)});
        }
      });

    flush_if_done(index);

    return index;
  }

  void index_router::drain(index_flushes flushes) { update_flush_counts(std::move(flushes)); }

  bool index_router::index_is_lowest_layer(data_cell_index_ptr const& index)
  {
    auto it = is_lowest_layer_hashes_.find(index->layer_hash());
    if (it != is_lowest_layer_hashes_.end()) {
      return it->second;
    }

    // Unknown layer hash: establish_layers() augments sorted_layer_paths_ with every (driver path,
    // unfold-produced descendant) so a hash absent from is_lowest_layer_hashes_ corresponds to a
    // layer the router was never told about.  Treating it as lowest is the safe default — skips
    // the rollup/expected-count bookkeeping that requires path knowledge — and matches the prior
    // behavior for unfold output layers.
    return is_lowest_layer_hashes_.emplace(index->layer_hash(), true).first->second;
  }

  detail::index_set_node_ptr index_router::index_set_node_for(data_cell_index_ptr const& index)
  {
    auto const layer_hash = index->layer_hash();
    if (auto it = index_set_node_cache_.find(layer_hash); it != index_set_node_cache_.end()) {
      return it->second;
    }

    layer_path const layerish_path{{index->layer_name()}};
    auto broadcaster = index_set_node_for(layerish_path);
    index_set_node_cache_.insert({layer_hash, broadcaster});
    return broadcaster;
  }

  auto index_router::index_set_node_for(layer_path const& layer_path) -> detail::index_set_node_ptr
  {
    std::vector<decltype(index_set_nodes_.begin())> candidates;
    for (auto it = index_set_nodes_.begin(), e = index_set_nodes_.end(); it != e; ++it) {
      if (layer_path.ends_with(it->first)) {
        candidates.push_back(it);
      }
    }

    if (candidates.size() == 1uz) {
      return candidates[0]->second;
    }

    if (candidates.empty()) {
      return nullptr;
    }

    std::string msg = fmt::format(
      "Multiple layers match specification {}:\n{}",
      layer_path,
      bulleted_list(candidates | std::views::transform([](auto const& it) { return it->first; })));
    throw std::runtime_error(msg);
  }

  std::pair<detail::multilayer_slots_ptr, detail::end_token_entries_ptr>
  index_router::multilayer_slots_for(data_cell_index_ptr const& index)
  {
    auto const layer_hash = index->layer_hash();

    // Fast path: shared lock allows concurrent reads of cached entries.
    {
      multilayer_slot_cache_const_accessor acc;
      if (multilayer_slot_cache_.find(acc, layer_hash)) {
        return {acc->second.message_slots, acc->second.end_token_entries};
      }
    }

    // Slow path: exclusive lock serializes concurrent cache misses for the same layer.
    multilayer_slot_cache_accessor acc;
    auto const inserted = multilayer_slot_cache_.insert(acc, layer_hash);
    if (not inserted) {
      return {acc->second.message_slots, acc->second.end_token_entries};
    }

    auto const layer_path = index->layer_path();
    detail::multilayer_slots message_slots;
    detail::end_token_entries end_token_entries;

    // For each multi-layer join node, determine which slots are relevant to this index.
    // Message entries:   All slots from a node are added if (1) at least one slot exactly matches
    //                    the current layer, and (2) all slots either exactly match or are parent
    //                    layers of the current index.
    // End-token entries: For each slot that exactly matches the current layer, consult the slot's
    //                    paired flush_spec to materialize one entry per path-aware counting-layer
    //                    descendant hash.  When the flush_spec's counting layer equals the slot's
    //                    routing layer (the common case), this resolves to exactly the routed
    //                    index's own layer_hash.  When they differ (a fold's partition slot), it
    //                    resolves to one entry per descendant of `layer_path` whose trailing layer
    //                    name equals the counting layer.
    for (auto& [node_name, node_slots] : multilayer_join_slots_) {
      auto const& slots = node_slots.slots;
      auto const& flush_specs = node_slots.flush_specs;
      assert(slots.size() == flush_specs.size());

      detail::multilayer_slots matching_slots;
      matching_slots.reserve(slots.size());

      bool has_exact_match = false;
      std::size_t matched_count = 0;

      for (std::size_t i = 0; i != slots.size(); ++i) {
        auto& slot = slots[i];
        auto const& flush = flush_specs[i];
        if (slot->matches_exactly(layer_path)) {
          has_exact_match = true;
          if (flush.counting_layer == slot->layer()) {
            // Counting layer is the routing layer: the routed index's own layer_hash is the unique
            // counting hash.
            end_token_entries.push_back(
              {.counting_layer_hash = layer_hash, .flush_port = flush.flush_port});
          } else {
            // Counting layer differs (fold partition slot).  Enumerate all descendant paths under
            // the routed partition path whose trailing name equals the counting layer; emit one
            // entry per descendant.
            auto const hashes = counting_layer_hashes_under(layer_path, flush.counting_layer);
            for (auto const& h : hashes) {
              end_token_entries.push_back(
                {.counting_layer_hash = h, .flush_port = flush.flush_port});
            }
          }
          matching_slots.push_back(slot);
          ++matched_count;
        } else if (slot->is_parent_of(index)) {
          matching_slots.push_back(slot);
          ++matched_count;
        }
      }

      // Add all matching slots to message entries only if we have an exact match and
      // all slots from this node matched something (either exactly or as a parent).
      if (has_exact_match and matched_count == slots.size()) {
        message_slots.insert(message_slots.end(),
                             std::make_move_iterator(matching_slots.begin()),
                             std::make_move_iterator(matching_slots.end()));
      }
    }

    acc->second.message_slots =
      std::make_shared<detail::multilayer_slots const>(std::move(message_slots));
    acc->second.end_token_entries =
      std::make_shared<detail::end_token_entries const>(std::move(end_token_entries));
    return {acc->second.message_slots, acc->second.end_token_entries};
  }

  std::vector<std::size_t> index_router::counting_layer_hashes_under(
    layer_path const& partition_layer_path, identifier const& counting_layer_name) const
  {
    std::vector<std::size_t> result;
    for (auto const& candidate : sorted_layer_paths_) {
      // candidate must be a strict descendant of the partition layer path
      if (not partition_layer_path.is_strict_prefix_of(candidate)) {
        continue;
      }
      // ...and its trailing segment must equal the counting layer name
      if (not candidate.ends_with(counting_layer_name)) {
        continue;
      }
      result.push_back(candidate.hash());
    }
    return result;
  }

  identifier index_router::deepest_layer_name(std::vector<identifier> const& layer_names) const
  {
    // Compute the maximum depth (= max path length) of any registered path whose trailing segment
    // matches each layer name.  The layer name with the greatest such depth is the most-derived
    // one.  Ties resolve lexicographically on the name itself for determinism — in practice ties
    // only occur when all slots are at the same depth, in which case `counting_layer == layer` is
    // correct for every slot and the tie resolution doesn't matter.
    auto depth_of = [this](identifier const& name) -> std::size_t {
      std::size_t best = 0;
      for (auto const& path : sorted_layer_paths_) {
        if (path.ends_with(name)) {
          std::size_t const depth = path.depth();
          if (depth > best) {
            best = depth;
          }
        }
      }
      return best;
    };

    assert(not layer_names.empty());
    auto const* result = &layer_names.front();
    std::size_t best_depth = depth_of(*result);
    for (auto const& name : layer_names | std::views::drop(1)) {
      auto const d = depth_of(name);
      if (d > best_depth or (d == best_depth and name < *result)) {
        result = &name;
        best_depth = d;
      }
    }
    return *result;
  }

  void index_router::update_flush_counts(index_flushes flushes)
  {
    for (auto const& [index, flush_counts] : flushes) {
      auto gate = gate_for(index);
      for (auto const& [child_layer_hash, count] : *flush_counts) {
        apply_expected_count(*gate, child_layer_hash, count.load());
      }
      flush_if_done(index);
    }
  }

  void index_router::apply_expected_count(flush_gate& gate,
                                          data_cell_index::hash_type const child_layer_hash,
                                          std::size_t const count)
  {
    // Non-lowest children contribute to the parent's readiness via rollup (roll_up_child() called
    // from flush_if_done()).  Lowest-layer children need no further accounting: their full count is
    // already reflected in the expected-count message and will be merged into committed_counts_ at
    // commit time.
    //
    // The pending counter must be bumped BEFORE update_expected_count() increments
    // received_flush_count_; otherwise a concurrent all_children_accounted() call could observe a
    // positive received count while the pending counter is still at its pre-bump value (which may
    // be at or below zero from earlier rollup notifications) and erroneously declare the tracker
    // ready.
    if (not is_lowest_layer_hash(child_layer_hash)) {
      gate.expect_child_rollups(static_cast<std::ptrdiff_t>(count));
    }
    gate.update_expected_count(child_layer_hash, count);
  }

  bool index_router::is_lowest_layer_hash(std::size_t const layer_hash) const
  {
    auto it = is_lowest_layer_hashes_.find(layer_hash);
    return it != is_lowest_layer_hashes_.end() ? it->second : true;
  }

  flush_gate_ptr index_router::gate_for(data_cell_index_ptr const& index)
  {
    // Fast path: entry already exists — read under shared lock to avoid serializing threads.
    const_accessor ca;
    if (flush_gates_.find(ca, index->hash())) {
      return ca->second;
    }
    ca.release();

    // Slow path: insert a new entry under exclusive lock.
    accessor a;
    if (flush_gates_.insert(a, index->hash())) {
      // Newly inserted — initialize the value.
      // If multiple unfolds consume this layer, the gate must wait for a flush message from each
      // of them before it can evaluate done().  Without this, the first unfold to finish could
      // cause the gate to fire before the others have reported their counts.
      std::size_t const expected_flush_count = [&]() -> std::size_t {
        auto it = unfold_count_per_input_layer_.find(index->layer_name());
        return it != unfold_count_per_input_layer_.end() ? it->second : 0;
      }();
      a->second = std::make_shared<flush_gate>(index, expected_flush_count);
    }
    return a->second;
  }

  void index_router::flush_if_done(data_cell_index_ptr index)
  {
    assert(index);

    while (index) {
      // Erase the entry while holding the exclusive accessor, then release the lock before calling
      // send_flush().  The erase claims exclusive ownership of this gate — any concurrent
      // flush_if_done call for the same index will fail to find the entry and return immediately,
      // preventing double-flush.
      flush_gate_ptr gate;
      {
        accessor a;
        if (not flush_gates_.find(a, index->hash())) {
          // This can happen when two threads process the same parent index, and one of them
          // releases it before the other completes.
          return;
        }

        if (not a->second->all_children_accounted()) {
          return;
        }

        gate = a->second;
        flush_gates_.erase(a);
      }

      gate->send_flush();

      auto next = index->parent();
      if (next) {
        gate_for(next)->roll_up_child(gate->committed_counts());
      }
      index = next;
    }
  }
}

#include "phlex/core/index_router.hpp"

#include "phlex/model/flush_gate.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <iterator>
#include <ranges>
#include <stdexcept>

using namespace phlex::experimental;

namespace {
  using layer_path_t = std::vector<std::string>;

  std::size_t layer_hash_for_path(layer_path_t const& layer_path)
  {
    std::size_t result = "job"_id.hash();
    for (auto const& layer_name : layer_path | std::views::drop(1)) {
      result = hash(result, identifier{layer_name}.hash());
    }
    return result;
  }

  bool is_strict_prefix(layer_path_t const& candidate, layer_path_t const& other)
  {
    // FIXME: Use std::ranges::starts_with(other, candidate) once the compilers support it (C++23)
    return candidate.size() < other.size() and
           std::ranges::mismatch(other, candidate).in2 == std::ranges::end(candidate);
  }

  std::string delimited_layer_path(std::string_view const layer_path)
  {
    if (not layer_path.starts_with("/")) {
      return fmt::format("/{}", layer_path);
    }
    return std::string{layer_path};
  }
}

namespace phlex::experimental {

  //========================================================================================
  // multilayer_slot implementation
  namespace detail {
    class multilayer_slot {
    public:
      multilayer_slot(tbb::flow::graph& g,
                      identifier layer,
                      tbb::flow::receiver<indexed_end_token>* flush_port,
                      tbb::flow::receiver<index_message>* input_port);

      void put_message(data_cell_index_ptr const& index, std::size_t message_id);
      void put_end_token(data_cell_index_ptr const& index, flush_gate const& fc);

      bool matches_exactly(std::string const& layer_path) const;
      bool is_parent_of(data_cell_index_ptr const& index) const;

    private:
      identifier layer_;
      index_set_node broadcaster_;
      flush_node flusher_;
    };

    multilayer_slot::multilayer_slot(tbb::flow::graph& g,
                                     identifier layer,
                                     tbb::flow::receiver<indexed_end_token>* flush_port,
                                     tbb::flow::receiver<index_message>* input_port) :
      layer_{std::move(layer)}, broadcaster_{g}, flusher_{g}
    {
      make_edge(broadcaster_, *input_port);
      make_edge(flusher_, *flush_port);
    }

    void multilayer_slot::put_message(data_cell_index_ptr const& index, std::size_t message_id)
    {
      if (layer_ == index->layer_name()) {
        broadcaster_.try_put({.index = index, .msg_id = message_id, .cache = false});
        return;
      }

      broadcaster_.try_put({.index = index->parent(layer_), .msg_id = message_id});
    }

    void multilayer_slot::put_end_token(data_cell_index_ptr const& index, flush_gate const& fc)
    {
      // We're going to have to be a little more careful about this.  The committed total count may
      // not be enough granularity for some downstream nodes.
      flusher_.try_put({.index = index, .count = static_cast<int>(fc.committed_total_count())});
    }

    bool multilayer_slot::matches_exactly(std::string const& layer_path) const
    {
      return layer_path.ends_with(delimited_layer_path(static_cast<std::string_view>(layer_)));
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
    unfold_flush_receiver_{g,
                           tbb::flow::unlimited,
                           [this](unfold_flush input) -> tbb::flow::continue_msg {
                             auto&& [index, layer_hash, count] = input;
                             apply_expected_count(*gate_for(index), layer_hash, count);
                             flush_if_done(index);
                             return {};
                           }},
    flusher_{g}
  {
  }

  void index_router::establish_layers(
    std::vector<std::vector<std::string>> const& layer_paths_from_driver,
    std::vector<identifier> unfold_input_layer_names,
    std::vector<identifier> unfold_output_layer_names)
  {
    auto sorted_layer_paths = layer_paths_from_driver;
    std::ranges::sort(sorted_layer_paths);

    // In sorted order, a path can only be a prefix of paths that follow it.
    for (std::size_t i = 0; i < sorted_layer_paths.size(); ++i) {
      bool const is_not_lowest_layer =
        i + 1 < sorted_layer_paths.size() and
        is_strict_prefix(sorted_layer_paths[i], sorted_layer_paths[i + 1]);
      if (is_not_lowest_layer) {
        auto const layer_hash = layer_hash_for_path(sorted_layer_paths[i]);
        is_lowest_layer_hashes_.emplace(layer_hash, false);
      }
    }

    unfold_input_layer_names_ = unfold_input_layer_names;
    unfold_output_layer_names_ = unfold_output_layer_names;
  }

  void index_router::finalize(tbb::flow::graph& g,
                              provider_input_ports_t provider_input_ports,
                              std::map<std::string, named_index_ports> multilayer_join_ports)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());

    // Create the index-set broadcast nodes for providers
    for (auto& [input_product, provider_port] : provider_input_ports | std::views::values) {
      auto [it, _] =
        index_set_nodes_.emplace(input_product.layer, std::make_shared<detail::index_set_node>(g));
      make_edge(*it->second, *provider_port);
    }

    for (auto const& [node_name, join_ports] : multilayer_join_ports) {
      spdlog::trace("Making multilayer slots for {}", node_name);
      detail::multilayer_slots slots;
      slots.reserve(join_ports.size());
      for (auto const& [layer, flush_port, input_port] : join_ports) {
        auto slot = std::make_shared<detail::multilayer_slot>(g, layer, flush_port, input_port);
        slots.push_back(slot);
      }
      multilayer_join_slots_.emplace(identifier{node_name}, std::move(slots));
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

    auto [message_slots, end_token_slots] = multilayer_slots_for(index);
    for (auto const& slot : *message_slots) {
      slot->put_message(index, message_id);
    }

    // Lowest-layer indices have no flush gate and contribute to their parent's readiness
    // solely through the expected-count message that announced them — there is nothing
    // to do here for them.
    if (is_lowest_layer) {
      return index;
    }

    gate_for(index)->set_flush_callback(
      [this, end_token_slots = std::move(end_token_slots), index, message_id](
        flush_gate const& fc) {
        for (auto const& slot : *end_token_slots) {
          slot->put_end_token(index, fc);
        }

        // Used only for folds, until folds use the slot infrastructure above.
        flusher_.try_put({index, fc.committed_counts(), message_id});
      });

    flush_if_done(index);

    return index;
  }

  void index_router::drain(index_flushes flushes) { update_flush_counts(std::move(flushes)); }

  void index_router::register_unfold_count_per_input_layer(std::map<identifier, std::size_t> counts)
  {
    // Called once during finalize(), before any indices are routed, so no concurrent access.
    unfold_count_per_input_layer_ = std::move(counts);
  }

  bool index_router::index_is_lowest_layer(data_cell_index_ptr const& index)
  {
    auto it = is_lowest_layer_hashes_.find(index->layer_hash());
    if (it != is_lowest_layer_hashes_.end()) {
      return it->second;
    }

    if (std::ranges::contains(unfold_input_layer_names_, index->layer_name())) {
      // FIXME: Need to make sure that the index is a child of existing layers
      return is_lowest_layer_hashes_.emplace(index->layer_hash(), false).first->second;
    }

    if (std::ranges::contains(unfold_output_layer_names_, index->layer_name())) {
      return is_lowest_layer_hashes_.emplace(index->layer_hash(), true).first->second;
    }

    // If the index is neither and input or an output to an unfold, it is assumed to be a lowest layer.
    return is_lowest_layer_hashes_.emplace(index->layer_hash(), true).first->second;
  }

  detail::index_set_node_ptr index_router::index_set_node_for(data_cell_index_ptr const& index)
  {
    auto const layer_hash = index->layer_hash();
    if (auto it = index_set_node_cache_.find(layer_hash); it != index_set_node_cache_.end()) {
      return it->second;
    }

    std::string const layerish_path{static_cast<std::string_view>(index->layer_name())};
    auto broadcaster = index_set_node_for(layerish_path);
    index_set_node_cache_.insert({layer_hash, broadcaster});
    return broadcaster;
  }

  auto index_router::index_set_node_for(std::string const& layer_path) -> detail::index_set_node_ptr
  {
    std::string const search_token = delimited_layer_path(layer_path);

    std::vector<decltype(index_set_nodes_.begin())> candidates;
    for (auto it = index_set_nodes_.begin(), e = index_set_nodes_.end(); it != e; ++it) {
      if (search_token.ends_with(delimited_layer_path(static_cast<std::string_view>(it->first)))) {
        candidates.push_back(it);
      }
    }

    if (candidates.size() == 1ull) {
      return candidates[0]->second;
    }

    if (candidates.empty()) {
      return nullptr;
    }

    std::string msg = fmt::format("Multiple layers match specification {}:\n", layer_path);
    for (auto const& it : candidates) {
      msg += fmt::format("\n- {}", it->first);
    }
    throw std::runtime_error(msg);
  }

  std::pair<detail::multilayer_slots_ptr, detail::multilayer_slots_ptr>
  index_router::multilayer_slots_for(data_cell_index_ptr const& index)
  {
    auto const layer_hash = index->layer_hash();

    // Fast path: shared lock allows concurrent reads of cached entries.
    {
      multilayer_slot_cache_const_accessor acc;
      if (multilayer_slot_cache_.find(acc, layer_hash)) {
        return {acc->second.message_slots, acc->second.end_token_slots};
      }
    }

    // Slow path: exclusive lock serializes concurrent cache misses for the same layer.
    multilayer_slot_cache_accessor acc;
    auto const inserted = multilayer_slot_cache_.insert(acc, layer_hash);
    if (not inserted) {
      return {acc->second.message_slots, acc->second.end_token_slots};
    }

    auto const layer_path = index->layer_path();
    detail::multilayer_slots message_slots;
    detail::multilayer_slots end_token_slots;

    // For each multi-layer join node, determine which slots are relevant to this index.
    // Message entries: All slots from a node are added if (1) at least one slot exactly
    //                  matches the current layer, and (2) all slots either exactly match
    //                  or are parent layers of the current index.
    // End-token entries: Only slots that exactly match the current layer are added.
    for (auto& [node_name, slots] : multilayer_join_slots_) {
      detail::multilayer_slots matching_slots;
      matching_slots.reserve(slots.size());

      bool has_exact_match = false;
      std::size_t matched_count = 0;

      for (auto& slot : slots) {
        if (slot->matches_exactly(layer_path)) {
          has_exact_match = true;
          end_token_slots.push_back(slot);
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
    acc->second.end_token_slots =
      std::make_shared<detail::multilayer_slots const>(std::move(end_token_slots));
    return {acc->second.message_slots, acc->second.end_token_slots};
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
    // Non-lowest children contribute to the parent's readiness via rollup
    // (roll_up_child() called from flush_if_done()).  Lowest-layer children need no
    // further accounting: their full count is already reflected in the expected-count
    // message and will be merged into committed_counts_ at commit time.
    //
    // The pending counter must be bumped BEFORE update_expected_count() increments
    // received_flush_count_; otherwise a concurrent all_children_accounted() call could
    // observe a positive received count while the pending counter is still at its
    // pre-bump value (which may be at or below zero from earlier rollup notifications)
    // and erroneously declare the tracker ready.
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
      // If multiple unfolds consume this layer, the gate must wait for a flush message
      // from each of them before it can evaluate done().  Without this, the first unfold to
      // finish could cause the gate to fire before the others have reported their counts.
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
      // Erase the entry while holding the exclusive accessor, then release the lock before
      // calling send_flush().  The erase claims exclusive ownership of this gate —
      // any concurrent flush_if_done call for the same index will fail to find the entry
      // and return immediately, preventing double-flush.
      flush_gate_ptr gate;
      {
        accessor a;
        if (not flush_gates_.find(a, index->hash())) {
          // This can happen when two threads process the same parent index,
          // and one of them releases it before the other completes.
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

#include "phlex/core/index_router.hpp"
#include "phlex/model/product_store.hpp"

#include "fmt/std.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stdexcept>

using namespace phlex::experimental;

namespace {
  auto delimited_layer_path(std::string layer_path)
  {
    if (not layer_path.starts_with("/")) {
      return "/" + layer_path;
    }
    return layer_path;
  }

  void send_messages(phlex::data_cell_index_ptr const& index,
                     std::size_t message_id,
                     phlex::experimental::detail::multilayer_slots const& slots)
  {
    for (auto& slot : slots) {
      slot->put_message(index, message_id);
    }
  }
}

namespace phlex::experimental {

  //========================================================================================
  // layer_scope implementation

  detail::layer_scope::layer_scope(flush_counters& counters,
                                   flusher_t& flusher,
                                   detail::multilayer_slots const& slots_for_layer,
                                   data_cell_index_ptr index,
                                   std::size_t const message_id) :
    counters_{counters},
    flusher_{flusher},
    slots_{slots_for_layer},
    index_{index},
    message_id_{message_id}
  {
    // FIXME: Only for folds right now
    counters_.update(index_);
  }

  detail::layer_scope::~layer_scope()
  {
    // To consider: We may want to skip the following logic if the framework prematurely
    //              needs to shut down.  Keeping it enabled allows in-flight folds to
    //              complete.  However, in some cases it may not be desirable to do this.

    for (auto& slot : slots_) {
      slot->put_end_token(index_);
    }

    // The following is for fold nodes only (temporary until the release of fold results are incorporated
    // into the above paradigm).
    auto flush_result = counters_.extract(index_);
    flush_counts_ptr result;
    if (not flush_result.empty()) {
      result = std::make_shared<flush_counts const>(std::move(flush_result));
    }
    flusher_.try_put({index_, std::move(result), message_id_});
  }

  std::size_t detail::layer_scope::depth() const { return index_->depth(); }

  //========================================================================================
  // multilayer_slot implementation

  detail::multilayer_slot::multilayer_slot(tbb::flow::graph& g,
                                           std::string layer,
                                           tbb::flow::receiver<indexed_end_token>* flush_port,
                                           tbb::flow::receiver<index_message>* input_port) :
    layer_{std::move(layer)}, broadcaster_{g}, flusher_{g}
  {
    make_edge(broadcaster_, *input_port);
    make_edge(flusher_, *flush_port);
  }

  void detail::multilayer_slot::put_message(data_cell_index_ptr const& index,
                                            std::size_t message_id)
  {
    if (layer_ == index->layer_name()) {
      broadcaster_.try_put({.index = index, .msg_id = message_id, .cache = false});
      return;
    }

    // Flush values are only used for indices that are *not* the "lowest" in the branch
    // of the hierarchy.
    ++counter_;
    broadcaster_.try_put({.index = index->parent(layer_), .msg_id = message_id});
  }

  void detail::multilayer_slot::put_end_token(data_cell_index_ptr const& index)
  {
    auto count = std::exchange(counter_, 0);
    if (count == 0) {
      // See comment above about flush values
      return;
    }

    flusher_.try_put({.index = index, .count = count});
  }

  bool detail::multilayer_slot::matches_exactly(std::string const& layer_path) const
  {
    return layer_path.ends_with(delimited_layer_path(layer_));
  }

  bool detail::multilayer_slot::is_parent_of(data_cell_index_ptr const& index) const
  {
    return index->parent(layer_) != nullptr;
  }

  //========================================================================================
  // index_router implementation

  index_router::index_router(tbb::flow::graph& g) : flusher_{g} {}

  void index_router::finalize(tbb::flow::graph& g,
                              provider_input_ports_t provider_input_ports,
                              std::map<std::string, named_index_ports> multilayers)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());
    provider_input_ports_ = std::move(provider_input_ports);

    // Create the index-set broadcast nodes for providers
    for (auto& [pq, provider_port] : provider_input_ports_ | std::views::values) {
      auto [it, _] =
        broadcasters_.try_emplace(pq.layer(), std::make_shared<detail::index_set_node>(g));
      make_edge(*it->second, *provider_port);
    }

    for (auto const& [node_name, multilayer] : multilayers) {
      spdlog::trace("Making multilayer caster for {}", node_name);
      detail::multilayer_slots casters;
      casters.reserve(multilayer.size());
      // FIXME: Consider whether the construction of casters can be simplied
      for (auto const& [layer, flush_port, input_port] : multilayer) {
        auto entry = std::make_shared<detail::multilayer_slot>(g, layer, flush_port, input_port);
        casters.push_back(entry);
      }
      multibroadcasters_.try_emplace(node_name, std::move(casters));
    }
  }

  data_cell_index_ptr index_router::route(data_cell_index_ptr const index)
  {
    backout_to(index);

    auto message_id = received_indices_.fetch_add(1);

    send_to_provider_index_nodes(index, message_id);
    auto const& slots_for_layer = send_to_multilayer_join_nodes(index, message_id);

    layers_.emplace(counters_, flusher_, slots_for_layer, index, message_id);

    return index;
  }

  void index_router::backout_to(data_cell_index_ptr const index)
  {
    assert(index);
    auto const new_depth = index->depth();
    while (not empty(layers_) and new_depth <= layers_.top().depth()) {
      layers_.pop();
    }
  }

  void index_router::drain()
  {
    while (not empty(layers_)) {
      layers_.pop();
    }
  }

  void index_router::send_to_provider_index_nodes(data_cell_index_ptr const& index,
                                                  std::size_t const message_id)
  {
    if (auto it = matched_broadcasters_.find(index->layer_hash());
        it != matched_broadcasters_.end()) {
      // Not all layers will have a corresponding broadcaster
      if (it->second) {
        it->second->try_put({.index = index, .msg_id = message_id});
      }
      return;
    }

    auto broadcaster = index_node_for(index->layer_name());
    if (broadcaster) {
      broadcaster->try_put({.index = index, .msg_id = message_id});
    }
    // We cache the result of the lookup even if there is no broadcaster for this layer,
    // to avoid repeated lookups for layers that don't have broadcasters.
    matched_broadcasters_.try_emplace(index->layer_hash(), broadcaster);
  }

  detail::multilayer_slots const& index_router::send_to_multilayer_join_nodes(
    data_cell_index_ptr const& index, std::size_t const message_id)
  {
    auto const layer_hash = index->layer_hash();

    if (auto it = matched_routing_entries_.find(layer_hash); it != matched_routing_entries_.end()) {
      send_messages(index, message_id, it->second);
      return matched_flushing_entries_.find(layer_hash)->second;
    }

    auto [routing_it, _] = matched_routing_entries_.try_emplace(layer_hash);
    auto [flushing_it, __] = matched_flushing_entries_.try_emplace(layer_hash);

    auto const layer_path = index->layer_path();

    // For each multi-layer join node, determine which slots are relevant to this index.
    // Routing entries: All slots from a node are added if (1) at least one slot exactly
    //                  matches the current layer, and (2) all slots either exactly match
    //                  or are parent layers of the current index.
    // Flushing entries: Only slots that exactly match the current layer are added.
    for (auto& [node_name, slots] : multibroadcasters_) {
      detail::multilayer_slots matching_slots;
      matching_slots.reserve(slots.size());

      bool has_exact_match = false;
      std::size_t matched_count = 0;

      for (auto& slot : slots) {
        if (slot->matches_exactly(layer_path)) {
          has_exact_match = true;
          flushing_it->second.push_back(slot);
          matching_slots.push_back(slot);
          ++matched_count;
        } else if (slot->is_parent_of(index)) {
          matching_slots.push_back(slot);
          ++matched_count;
        }
      }

      // Add all matching slots to routing entries only if we have an exact match and
      // all slots from this node matched something (either exactly or as a parent).
      if (has_exact_match and matched_count == slots.size()) {
        routing_it->second.insert(routing_it->second.end(),
                                  std::make_move_iterator(matching_slots.begin()),
                                  std::make_move_iterator(matching_slots.end()));
      }
    }
    send_messages(index, message_id, routing_it->second);
    return flushing_it->second;
  }

  auto index_router::index_node_for(std::string const& layer_path) -> detail::index_set_node_ptr
  {
    std::string const search_token = delimited_layer_path(layer_path);

    std::vector<decltype(broadcasters_.begin())> candidates;
    for (auto it = broadcasters_.begin(), e = broadcasters_.end(); it != e; ++it) {
      if (search_token.ends_with(delimited_layer_path(it->first))) {
        candidates.push_back(it);
      }
    }

    if (candidates.size() == 1ull) {
      return candidates[0]->second;
    }

    if (candidates.empty()) {
      return nullptr;
    }

    std::string msg{"Multiple layers match specification " + layer_path + ":\n"};
    for (auto const& it : candidates) {
      msg += "\n- " + it->first;
    }
    throw std::runtime_error(msg);
  }
}

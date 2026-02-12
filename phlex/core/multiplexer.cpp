#include "phlex/core/multiplexer.hpp"
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
}

namespace phlex::experimental {

  //========================================================================================
  // layer_sentry implementation

  layer_sentry::layer_sentry(flush_counters& counters,
                             flusher_t& flusher,
                             detail::multilayer_sender_ptrs_t const& senders_for_layer,
                             data_cell_index_ptr index,
                             std::size_t const message_id) :
    counters_{counters},
    flusher_{flusher},
    senders_{senders_for_layer},
    index_{index},
    message_id_{message_id}
  {
    // FIXME: Only for folds right now
    counters_.update(index_);
  }

  layer_sentry::~layer_sentry()
  {
    // To consider: We may want to skip the following logic if the framework prematurely
    //              needs to shut down.  Keeping it enabled allows in-flight folds to
    //              complete.  However, in some cases it may not be desirable to do this.

    for (auto& sender : senders_) {
      sender->put_end_token(index_);
    }

    // =====================================================================================
    // For fold nodes only (temporary until the release of fold results are incorporated
    // into the above paradigm).
    auto flush_result = counters_.extract(index_);
    flush_counts_ptr result;
    if (not flush_result.empty()) {
      result = std::make_shared<flush_counts const>(std::move(flush_result));
    }
    flusher_.try_put({index_, std::move(result), message_id_});
  }

  std::size_t layer_sentry::depth() const { return index_->depth(); }

  //========================================================================================
  // multiplexer implementation

  multiplexer::multiplexer(tbb::flow::graph& g) : flusher_{g} {}

  void multiplexer::finalize(tbb::flow::graph& g,
                             provider_input_ports_t provider_input_ports,
                             std::map<std::string, named_index_ports> multilayers)
  {
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());
    provider_input_ports_ = std::move(provider_input_ports);

    // Create the index-set broadcast nodes for providers
    for (auto& [pq, provider_port] : provider_input_ports_ | std::views::values) {
      auto [it, _] = broadcasters_.try_emplace(pq.layer(), g);
      make_edge(it->second, *provider_port);
    }

    for (auto const& [node_name, multilayer] : multilayers) {
      spdlog::trace("Making multilayer caster for {}", node_name);
      multibroadcaster_entries casters;
      casters.reserve(multilayer.size());
      for (auto const& [layer, flush_port, input_port] : multilayer) {
        auto& entry = casters.emplace_back(layer, detail::index_set_node{g}, detail::flush_node{g});
        make_edge(entry.broadcaster, *input_port); // Connect with index ports of multi-algorithms
        make_edge(entry.flusher, *flush_port);     // Connect with flush ports of multi-algorithms
      }
      multibroadcasters_.try_emplace(node_name, std::move(casters));
    }
  }

  data_cell_index_ptr multiplexer::route(data_cell_index_ptr const index)
  {
    backout_to(index);

    auto message_id = received_indices_.fetch_add(1);

    send_to_provider_index_nodes(index, message_id);
    auto const& senders_for_layer = send_to_multilayer_join_nodes(index, message_id);

    layers_.emplace(counters_, flusher_, senders_for_layer, index, message_id);

    return index;
  }

  void multiplexer::backout_to(data_cell_index_ptr const index)
  {
    assert(index);
    auto const new_depth = index->depth();
    while (not empty(layers_) and new_depth <= layers_.top().depth()) {
      layers_.pop();
    }
  }

  void multiplexer::drain()
  {
    while (not empty(layers_)) {
      layers_.pop();
    }
  }

  void multiplexer::send_to_provider_index_nodes(data_cell_index_ptr const& index,
                                                 std::size_t const message_id)
  {
    if (auto it = cached_broadcasters_.find(index->layer_hash());
        it != cached_broadcasters_.end()) {
      // Not all layers will have a corresponding broadcaster
      if (it->second) {
        it->second->try_put({.index = index, .msg_id = message_id});
      }
      return;
    }

    auto* broadcaster = index_node_for(index->layer_name());
    if (broadcaster) {
      broadcaster->try_put({.index = index, .msg_id = message_id});
    }
    // We cache the result of the lookup even if there is no broadcaster for this layer,
    // to avoid repeated lookups for layers that don't have broadcasters.
    cached_broadcasters_.try_emplace(index->layer_hash(), broadcaster);
  }

  detail::multilayer_sender_ptrs_t const& multiplexer::send_to_multilayer_join_nodes(
    data_cell_index_ptr const& index, std::size_t const message_id)
  {
    auto const layer_hash = index->layer_hash();

    auto do_the_put = [](data_cell_index_ptr const& index,
                         std::size_t message_id,
                         detail::multilayer_sender_ptrs_t& nodes) {
      for (auto& sender : nodes) {
        sender->put_message(index, message_id);
      }
    };

    if (auto it = cached_multicasters_.find(layer_hash); it != cached_multicasters_.end()) {
      do_the_put(index, message_id, it->second);
      return cached_casters_for_flushing_.find(layer_hash)->second;
    }

    auto it = cached_multicasters_.try_emplace(layer_hash).first;
    auto it2 = cached_casters_for_flushing_.try_emplace(layer_hash).first;

    auto const layer_path = index->layer_path();

    // spdlog::info("Making multibroadcaster for {}", index->layer_path());

    detail::multilayer_sender_ptrs_t senders_for_flushing;
    for (auto& [multilayer_str, entries] : multibroadcasters_) {
      detail::multilayer_sender_ptrs_t senders;
      senders.reserve(entries.size());
      bool name_in_multilayer = false;
      for (auto& [layer, caster, flusher] : entries) {
        std::string const search_token = delimited_layer_path(layer);
        auto sender = std::make_shared<detail::multilayer_sender>(layer, &caster, &flusher);
        if (layer_path.ends_with(search_token)) {
          senders.push_back(sender);
          senders_for_flushing.push_back(sender);
          name_in_multilayer = true;
        } else if (index->parent(layer)) {
          senders.push_back(sender);
        }
        // FIXME: Can this support a product_query's layer specification like "/job/run"?
      }

      if (name_in_multilayer and senders.size() == entries.size()) {
        it->second.insert(it->second.end(),
                          std::make_move_iterator(senders.begin()),
                          std::make_move_iterator(senders.end()));
      }
    }
    do_the_put(index, message_id, it->second);
    return it2->second;
  }

  auto multiplexer::index_node_for(std::string const& layer_path) -> detail::index_set_node*
  {
    std::string const search_token = delimited_layer_path(layer_path);

    std::vector<broadcasters_t::iterator> candidates;
    for (auto it = broadcasters_.begin(), e = broadcasters_.end(); it != e; ++it) {
      if (search_token.ends_with(delimited_layer_path(it->first))) {
        candidates.push_back(it);
      }
    }

    if (candidates.size() == 1ull) {
      return &candidates[0]->second;
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

  detail::multilayer_sender::multilayer_sender(std::string const& layer,
                                               index_set_node* broadcaster,
                                               flush_node* flusher) :
    layer_{layer}, broadcaster_{broadcaster}, flusher_{flusher}
  {
  }

  void detail::multilayer_sender::put_message(data_cell_index_ptr const& index,
                                              std::size_t message_id)
  {
    if (layer_ == index->layer_name()) {
      broadcaster_->try_put({.msg_id = message_id, .index = index, .cache = false});
      return;
    }

    // Flush values are only used for indices that are *not* the "lowest" in the branch
    // of the hierarchy.
    ++counter_;
    broadcaster_->try_put({.msg_id = message_id, .index = index->parent(layer_)});
  }

  void detail::multilayer_sender::put_end_token(data_cell_index_ptr const& index)
  {
    auto count = std::exchange(counter_, 0);
    if (count == 0) {
      // See comment above about flush values
      return;
    }

    flusher_->try_put({.index = index, .count = count});
  }
}

#include "index_router.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <stdexcept>
#include <utility>

using namespace tbb;
using namespace phlex;

test::index_router::index_router(flow::graph& g,
                                 std::vector<std::string> layers,
                                 std::map<std::string, named_index_ports> multilayers)
{
  for (auto const& layer : layers) {
    broadcasters_.try_emplace(layer, g);
  }
  for (auto const& [node_name, multilayer] : multilayers) {
    spdlog::trace("Making multilayer caster for {}", node_name);
    multibroadcaster_entries casters;
    casters.reserve(multilayer.size());
    for (auto const& [layer, flush_port, input_port] : multilayer) {
      auto& entry = casters.emplace_back(layer, index_set_node{g}, flush_node{g});
      make_edge(entry.broadcaster, *input_port); // Connect with index ports of multi-algorithms
      make_edge(entry.flusher, *flush_port);     // Connect with flush ports of multi-algorithms
    }
    multibroadcasters_.try_emplace(node_name, std::move(casters));
  }
}

void test::index_router::shutdown()
{
  backout_to(data_cell_index::base_ptr());
  last_index_ = nullptr;
}

void test::index_router::route(data_cell_index_ptr const& index)
{
  backout_to(index);
  auto msg_id = counter_.fetch_add(1);
  send(index, msg_id);
  multisend(index, msg_id);
  last_index_ = index;
}

void test::index_router::backout_to(data_cell_index_ptr const& index)
{
  assert(index);

  if (!last_index_) {
    // This happens when we encounter the first index
    return;
  }

  if (index->parent() == last_index_->parent()) {
    // At the same level in the hierarchy
    return;
  }

  if (index->parent(last_index_->layer_name())) {
    // Descending further into the hierarchy
    return;
  }

  // What's left is situations where we need to go up the hierarchy chain.

  auto do_the_put = [this](data_cell_index_ptr const& index) {
    // FIXME: This lookup should be fixed
    for (auto& [_, senders] : cached_multicasters_) {
      for (auto& sender : senders) {
        if (sender.layer() == index->layer_name()) {
          sender.put_end_token(index);
        }
      }
    }
  };

  auto current = last_index_;
  while (current and current->layer_hash() != index->layer_hash()) {
    do_the_put(current);
    current = current->parent();
    assert(current); // Cannot be non-null
  }
  do_the_put(current);
}

auto test::index_router::index_node_for(std::string const& layer) -> index_set_node&
{
  std::vector<broadcasters_t::iterator> candidates;
  for (auto it = broadcasters_.begin(), e = broadcasters_.end(); it != e; ++it) {
    if (it->first.ends_with("/" + layer)) {
      candidates.push_back(it);
    }
  }

  if (candidates.size() == 1ull) {
    return candidates[0]->second;
  }

  if (candidates.empty()) {
    throw std::runtime_error("No broadcaster found for layer specification \"" + layer + "\"");
  }

  std::string msg{"Multiple layers match specification " + layer + ":\n"};
  for (auto const& it : candidates) {
    msg += "\n- " + it->first;
  }
  throw std::runtime_error(msg);
}

void test::index_router::send(data_cell_index_ptr const& index, std::size_t message_id)
{
  auto it = broadcasters_.find(index->layer_path());
  assert(it != broadcasters_.end());
  it->second.try_put({.msg_id = message_id, .index = index});
}

void test::index_router::multisend(data_cell_index_ptr const& index, std::size_t message_id)
{
  auto const layer_hash = index->layer_hash();
  // spdlog::trace("Multilayer send for layer hash {} {}", layer_hash, index->to_string());

  auto do_the_put = [](data_cell_index_ptr const& index,
                       std::size_t message_id,
                       std::vector<multilayer_sender>& nodes) {
    for (auto& sender : nodes) {
      sender.put_message(index, message_id);
    }
  };

  if (auto it = cached_multicasters_.find(layer_hash); it != cached_multicasters_.end()) {
    do_the_put(index, message_id, it->second);
    return;
  }

  auto [it, _] = cached_multicasters_.try_emplace(layer_hash);

  // spdlog::trace("Assigning new multi-caster for {} (path: {})", layer_hash, index->layer_path());
  for (auto& [multilayer_str, entries] : multibroadcasters_) {
    // Now we need to check how to match "ports" and the multilayer
    std::vector<multilayer_sender> senders;
    senders.reserve(entries.size());
    bool name_in_multilayer = false;
    for (auto& [layer, caster, flusher] : entries) {
      if (layer == index->layer_name()) {
        senders.emplace_back(layer, &caster, &flusher);
        name_in_multilayer = true;
      } else if (index->parent(layer)) {
        senders.emplace_back(layer, &caster, &flusher);
      }
    }

    if (name_in_multilayer and senders.size() == entries.size()) {
      // spdlog::trace("Match for {}: {} (path: {})", multilayer_str, layer_hash, index->layer_path());
      it->second.insert(it->second.end(),
                        std::make_move_iterator(senders.begin()),
                        std::make_move_iterator(senders.end()));
    }
  }
  // if (it->second.empty()) {
  //   spdlog::trace("No broadcasters for {}", layer_hash);
  // } else {
  //   spdlog::trace("Number of broadcasters for {}: {}", layer_hash, it->second.size());
  // }
  do_the_put(index, message_id, it->second);
}

test::index_router::multilayer_sender::multilayer_sender(std::string const& layer,
                                                         index_set_node* broadcaster,
                                                         flush_node* flusher) :
  layer_{layer}, broadcaster_{broadcaster}, flusher_{flusher}
{
}

void test::index_router::multilayer_sender::put_message(data_cell_index_ptr const& index,
                                                        std::size_t message_id)
{
  if (layer_ == index->layer_name()) {
    broadcaster_->try_put({.msg_id = message_id, .index = index, .cache = false});
    return;
  }

  // Flush values are needed only used for indices that are *not* the "lowest" in the branch
  // of the hierarchy.
  ++counter_;
  broadcaster_->try_put({.msg_id = message_id, .index = index->parent(layer_)});
}

void test::index_router::multilayer_sender::put_end_token(data_cell_index_ptr const& index)
{
  auto count = std::exchange(counter_, 0);
  if (count == 0) {
    // See comment above about flush values
    return;
  }

  flusher_->try_put({.index = index, .count = count});
}

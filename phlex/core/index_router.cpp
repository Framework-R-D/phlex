#include "phlex/core/index_router.hpp"

#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_gate.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/bulleted_list.hpp"
#include "phlex/utilities/signed_size.hpp"

#include <fmt/format.h>
#include <gsl/assert>
#include <oneapi/tbb/flow_graph.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using phlex::experimental::identifier;
using phlex::experimental::layer_path;

namespace phlex::detail {
  namespace {
    // Build the complete set of paths implied by the driver's declarations by adding each path's
    // intermediate parents. Incomplete paths are rooted at /job; for example, declarations
    // {"/job/run/spill", "spill"} produce /job, /job/run, /job/run/spill, and /job/spill.
    std::map<layer_path, std::optional<std::string>> driver_layer_paths(
      std::vector<layer_path> const& layer_paths_from_driver)
    {
      std::map<layer_path, std::optional<std::string>> paths{{layer_path{"/job"}, std::nullopt}};
      for (auto const& path : layer_paths_from_driver) {
        // Driver declarations may contain only leaves. Include their intermediate parents too.
        std::vector<identifier> prefix;
        if (not path.is_complete()) {
          prefix.emplace_back("job");
        }
        for (auto const& component : path.components()) {
          prefix.push_back(component);
          paths.try_emplace(layer_path{prefix}, std::nullopt);
        }
      }
      return paths;
    }

    bool matches_input_chain(layer_path const& path, std::vector<identifier> const& input_layers)
    {
      Expects(not input_layers.empty());

      // At least one of the input layers must equal the path's last component.
      if (std::ranges::none_of(input_layers,
                               [&path](auto const& input) { return path.ends_with(input); })) {
        return false;
      }

      // Check that all input layers are present in the path.
      return std::ranges::all_of(input_layers,
                                 [&path](auto const& input) { return path.contains(input); });
    }

    // Select strict descendant invocation paths whose nearest-name projection is this
    // partition. Full path hashes keep repeated names on branches and within chains distinct.
    std::vector<std::size_t> counting_layer_hashes_under(
      layer_path const& partition_layer_path, internal::join_node_slots const& node_slots)
    {
      std::vector<std::size_t> result;
      for (auto const& candidate : node_slots.invocation_paths) {
        // candidate must be a strict descendant of the partition layer path
        if (not partition_layer_path.is_strict_prefix_of(candidate)) {
          continue;
        }
        // Routing projects to the nearest ancestor with this storage name. A deeper
        // occurrence shadows this partition, which therefore owns none of its invocations.
        auto const descendants = candidate.components().subspan(partition_layer_path.depth());
        if (std::ranges::contains(descendants, partition_layer_path.components().back())) {
          continue;
        }
        result.push_back(candidate.hash());
      }
      return result;
    }
  }

  //========================================================================================
  // multilayer_slot implementation
  //
  // A static participant slot projects invocation indices to its storage layer and carries
  // the receiver that completes cached products or accumulators against the join's domain.
  namespace internal {
    class multilayer_slot {
    public:
      multilayer_slot(tbb::flow::graph& g, join_participant const& participant, bool cached);

      void put_message(data_cell_index_ptr const& index, std::size_t message_id);

      bool matches_exactly(layer_path const& path) const;
      bool cached() const { return cached_; }
      tbb::flow::receiver<indexed_end_token>* completion_port() const { return completion_port_; }

    private:
      identifier layer_;
      index_set_node broadcaster_;
      tbb::flow::receiver<indexed_end_token>* completion_port_;
      bool cached_;
    };

    multilayer_slot::multilayer_slot(tbb::flow::graph& g,
                                     join_participant const& participant,
                                     bool cached) :
      layer_{participant.layer},
      broadcaster_{g},
      completion_port_{participant.completion_port},
      cached_{cached}
    {
      make_edge(broadcaster_, *participant.index_port);
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
  } // namespace internal

  namespace {
    struct join_slot_resolution {
      internal::multilayer_slots message_slots;
      internal::end_token_entries end_token_entries;
    };

    // Route all participants only for an eligible invocation. At a participant's partition,
    // combine the same domain's strict descendant counts into one completion token. A deepest
    // product passes through and receives no token; applicable empty partitions still get zero.
    join_slot_resolution resolve_join_slots(layer_path const& layer_path,
                                            internal::join_node_slots const& node_slots)
    {
      auto const& slots = node_slots.slots;
      internal::multilayer_slots message_slots;
      internal::end_token_entries end_token_entries;

      if (std::ranges::contains(node_slots.invocation_paths, layer_path)) {
        message_slots = slots;
        return {.message_slots = std::move(message_slots), .end_token_entries = {}};
      }

      for (auto const& slot : slots) {
        if (slot->cached() and slot->matches_exactly(layer_path)) {
          // Name-based product wiring also supplies excluded storage cells. They need a
          // zero completion even when no eligible invocation projects to this cell.
          end_token_entries.push_back(
            {.counting_layer_hashes = counting_layer_hashes_under(layer_path, node_slots),
             .flush_port = slot->completion_port()});
        }
      }
      return {.message_slots = std::move(message_slots),
              .end_token_entries = std::move(end_token_entries)};
    }
  } // namespace

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
      g, tbb::flow::unlimited, [this](unfold_flush const& input) -> tbb::flow::continue_msg {
        auto const& [index, layer_hash, count] = input;
        apply_expected_count(*gate_for(index), layer_hash, count);
        flush_if_done(index);
        return {};
      }}
  {
  }

  void index_router::finalize(tbb::flow::graph& g,
                              std::vector<layer_path> const& layer_paths_from_driver,
                              unfold_data const& unfolds,
                              provider_input_ports_t provider_input_ports,
                              fold_partition_ports_t fold_partition_ports,
                              std::map<std::string, join_registration> const& multilayer_join_ports)
  {
    using namespace phlex::experimental::literals;
    // We must have at least one provider port, or there can be no data to process.
    assert(!provider_input_ports.empty());

    establish_layer_hierarchy(layer_paths_from_driver, unfolds);
    wire_provider_index_sets(g, std::move(provider_input_ports));
    wire_fold_partition_index_sets(g, std::move(fold_partition_ports));
    build_multilayer_join_slots(g, multilayer_join_ports);
  }

  namespace {
    struct unfold_layer_paths {
      std::vector<layer_path> sorted_paths;
      std::map<std::size_t, std::size_t> number_unfolds_per_parent_path;
    };

    // Resolve each unfold on compatible ancestor chains. Its deepest input is the only parent
    // that receives its flush, so paths and expectations must be derived from the same application.
    // Visit each discovered path once and append new children to the worklist, allowing chained
    // unfolds to resolve even when a dependent is registered before its producer.
    // An acyclic chain adds at most one layer per unfold node; expansion beyond the initial deepest
    // path plus the number of unfolds indicates a cycle and fails with the offending node name.
    unfold_layer_paths expand_unfold_layer_paths(
      std::vector<layer_path> const& layer_paths_from_driver,
      index_router::unfold_data const& unfolds)
    {
      unfold_layer_paths result;
      auto paths = driver_layer_paths(layer_paths_from_driver);
      auto layer_paths = paths | std::views::keys | std::ranges::to<std::vector>();

      assert(not layer_paths.empty());
      std::size_t const initial_deepest_path_depth =
        std::ranges::max_element(layer_paths, {}, &layer_path::depth)->depth();
      std::size_t const max_allowed_depth = initial_deepest_path_depth + unfolds.size();

      // N.B. layer_paths.size() is purposefully evaluated on each iteration, since new children
      // are appended to the worklist.
      for (std::size_t i = 0; i < layer_paths.size(); ++i) {
        // Appending children can reallocate the worklist, so keep a copy of the parent.
        auto const parent_path = layer_paths[i];
        for (auto const& unfold : unfolds) {
          if (not matches_input_chain(parent_path, unfold.input_layers)) {
            continue;
          }
          auto components = parent_path.components() | std::ranges::to<std::vector>();
          components.push_back(unfold.output_layer);
          layer_path candidate{std::move(components)};
          if (candidate.depth() > max_allowed_depth) {
            throw std::runtime_error(fmt::format("Unfold layer hierarchy expansion exceeded max "
                                                 "depth {} (initial deepest {} + {} unfold "
                                                 "node(s)). Offending unfold: {}",
                                                 max_allowed_depth,
                                                 initial_deepest_path_depth,
                                                 unfolds.size(),
                                                 unfold.name));
          }
          auto const [producer, inserted] = paths.try_emplace(candidate, unfold.name);
          if (not inserted) {
            if (not producer->second) {
              throw std::runtime_error(fmt::format(
                "Unfold '{}' specifies output layer '{}' already declared by the driver.",
                unfold.name,
                candidate));
            }
            throw std::runtime_error(
              fmt::format("Unfolds '{}' and '{}' specify the same output layer '{}'.",
                          *producer->second,
                          unfold.name,
                          candidate));
          }
          ++result.number_unfolds_per_parent_path[parent_path.hash()];
          layer_paths.push_back(std::move(candidate));
        }
      }
      result.sorted_paths.assign_range(paths | std::views::keys);
      return result;
    }
  } // namespace

  // --------------------------------------------------------------------------------------------
  void index_router::establish_layer_hierarchy(
    std::vector<layer_path> const& layer_paths_from_driver, unfold_data const& unfolds)
  {
    auto result = expand_unfold_layer_paths(layer_paths_from_driver, unfolds);
    sorted_layer_paths_ = std::move(result.sorted_paths);
    number_unfolds_per_parent_path_ = std::move(result.number_unfolds_per_parent_path);
    assert(!sorted_layer_paths_.empty());

    // In sorted order, a path can only be a prefix of paths that follow it.
    for (std::size_t i = 0; i < sorted_layer_paths_.size(); ++i) {
      auto const layer_hash = sorted_layer_paths_[i].hash();
      bool const is_lowest_layer =
        i + 1 == sorted_layer_paths_.size() or
        not sorted_layer_paths_[i].is_strict_prefix_of(sorted_layer_paths_[i + 1]);
      // Record every known layer, both lowest and non-lowest.  Pre-populating the lowest entries
      // lets index_is_lowest_layer() return a definitive answer without its unknown-path fallback,
      // including for unfold outputs that themselves parent another unfold's children.
      is_lowest_layer_hashes_.emplace(layer_hash, is_lowest_layer);
    }
  }

  void index_router::wire_provider_index_sets(tbb::flow::graph& g,
                                              provider_input_ports_t provider_input_ports)
  {
    for (auto& [input_product, provider_port] : provider_input_ports | std::views::values) {
      auto [it, _] = index_set_nodes_.emplace(input_product.layer ? *input_product.layer : "*"_id,
                                              std::make_shared<internal::index_set_node>(g));
      make_edge(*it->second, *provider_port);
    }
  }

  void index_router::wire_fold_partition_index_sets(tbb::flow::graph& g,
                                                    fold_partition_ports_t fold_partition_ports)
  {
    for (auto& [layer, port] : fold_partition_ports | std::views::values) {
      auto [it, _] = index_set_nodes_.emplace(layer, std::make_shared<internal::index_set_node>(g));
      make_edge(*it->second, *port);
    }
  }

  // --------------------------------------------------------------------------------------------
  // Product inputs drive invocations only on paths satisfying every participant's ancestry.
  // Resolve the domain once so routing and completion cannot select different branches.
  void index_router::build_multilayer_join_slots(
    tbb::flow::graph& g, std::map<std::string, join_registration> const& multilayer_join_ports)
  {
    for (auto const& [node_name, registration] : multilayer_join_ports) {
      assert(not registration.participants.empty());
      assert(not registration.input_layers.empty());

      spdlog::trace("Making multilayer slots for {}", node_name);

      internal::join_node_slots node_slots;
      for (auto const& path : sorted_layer_paths_) {
        if (matches_input_chain(path, registration.input_layers) and
            std::ranges::all_of(registration.participants, [&path](auto const& participant) {
              return path.contains(participant.layer);
            })) {
          node_slots.invocation_paths.push_back(path);
        }
      }
      node_slots.slots.reserve(registration.participants.size());
      for (auto const& participant : registration.participants) {
        bool const pass_through =
          std::ranges::any_of(node_slots.invocation_paths,
                              [&](auto const& p) { return p.ends_with(participant.layer); });
        bool const cached = std::ranges::any_of(node_slots.invocation_paths, [&](auto const& p) {
          return not p.ends_with(participant.layer);
        });
        // A repeater's caching mode is fixed, even when the same layer name occurs on
        // multiple branches. Do not register a domain requiring both modes for one slot.
        if (pass_through and cached) {
          throw std::runtime_error(
            fmt::format("Join '{}' participant '{}' requires mixed caching and pass-through modes.",
                        node_name,
                        participant.layer));
        }
        node_slots.slots.push_back(
          std::make_shared<internal::multilayer_slot>(g, participant, cached));
      }
      multilayer_join_slots_.emplace(identifier{node_name}, std::move(node_slots));
    }
  }

  data_cell_index_ptr index_router::route(data_cell_index_ptr const& index,
                                          index_flushes const& flushes)
  {
    update_flush_counts(flushes);
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

    // Lowest-layer indices have no flush gate. Excluded storage cells can nevertheless
    // receive products in a globally cached participant, so retire those with a zero token.
    if (is_lowest_layer) {
      for (auto const& entry : *end_token_entries) {
        assert(entry.counting_layer_hashes.empty());
        entry.flush_port->try_put({.index = index, .count = 0});
      }
      return index;
    }

    gate_for(index)->set_flush_callback(
      [end_token_entries = std::move(end_token_entries)](flush_gate const& fc) {
        for (auto const& entry : *end_token_entries) {
          signed_size_t count = 0;
          for (auto const hash : entry.counting_layer_hashes) {
            count += fc.committed_count_for_layer(hash);
          }
          entry.flush_port->try_put({.index = fc.index(), .count = count});
        }
      });

    flush_if_done(index);

    return index;
  }

  void index_router::drain(index_flushes const& flushes) { update_flush_counts(flushes); }

  bool index_router::index_is_lowest_layer(data_cell_index_ptr const& index)
  {
    auto it = is_lowest_layer_hashes_.find(index->layer_hash());
    if (it != is_lowest_layer_hashes_.end()) {
      return it->second;
    }

    // Unknown layer hash: establish_layer_hierarchy() includes driver paths and unfold-produced
    // descendants, so a hash absent from is_lowest_layer_hashes_ corresponds to a layer the router
    // was never told about. Treating it as lowest skips the rollup/expected-count bookkeeping that
    // requires path knowledge and matches the prior behavior for unfold output layers.
    return is_lowest_layer_hashes_.emplace(index->layer_hash(), true).first->second;
  }

  internal::index_set_node_ptr index_router::index_set_node_for(data_cell_index_ptr const& index)
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

  auto index_router::index_set_node_for(layer_path const& layer_path)
    -> internal::index_set_node_ptr
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

    std::string const msg = fmt::format(
      "Multiple layers match specification {}:\n{}",
      layer_path,
      bulleted_list(candidates | std::views::transform([](auto const& it) { return it->first; })));
    throw std::runtime_error(msg);
  }

  std::pair<internal::multilayer_slots_ptr, internal::end_token_entries_ptr>
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
    internal::multilayer_slots message_slots;
    internal::end_token_entries end_token_entries;

    for (auto const& node_slots : multilayer_join_slots_ | std::views::values) {
      auto [resolved_message_slots, resolved_end_token_entries] =
        resolve_join_slots(layer_path, node_slots);
      message_slots.append_range(std::views::as_rvalue(resolved_message_slots));
      end_token_entries.append_range(std::views::as_rvalue(resolved_end_token_entries));
    }

    acc->second = {
      .message_slots = std::make_shared<internal::multilayer_slots const>(std::move(message_slots)),
      .end_token_entries =
        std::make_shared<internal::end_token_entries const>(std::move(end_token_entries))};
    return {acc->second.message_slots, acc->second.end_token_entries};
  }

  void index_router::update_flush_counts(index_flushes const& flushes)
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
      gate.expect_child_rollups(count);
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
        auto it = number_unfolds_per_parent_path_.find(index->layer_hash());
        return it != number_unfolds_per_parent_path_.end() ? it->second : 0;
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

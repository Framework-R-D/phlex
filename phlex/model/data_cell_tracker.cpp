#include "phlex/model/data_cell_tracker.hpp"

#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/hashing.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
  auto make_data_cell_counts(phlex::data_cell_index_ptr const& index)
  {
    auto result = std::make_shared<phlex::detail::data_cell_counts>();
    result->emplace(index->layer_hash(), 1);
    return result;
  }
}

namespace phlex::detail {
  data_cell_tracker::~data_cell_tracker()
  {
    if (pending_flushes_.empty()) {
      return;
    }
    spdlog::warn("Cached pending flushes at destruction:");
    for (auto const& [index, flush_counts] : pending_flushes_ | std::views::values) {
      spdlog::warn("  Index: {}", index->to_string());
      for (auto const& [layer_hash, count] : *flush_counts) {
        spdlog::warn("    {} = {}", layer_hash, count.load());
      }
    }
  }

  index_flushes data_cell_tracker::report_and_evict_ready_flushes(
    data_cell_index_ptr const& received_index)
  {
    // Always update the cached index.  The logic below uses the previous cached index to
    // determine what flushes to emit.
    auto cached_index = std::exchange(cached_index_, received_index);

    // Just beginning job (or ending a job that immediately threw an exception)
    if (cached_index == nullptr) {
      return {};
    }

    // Ending job.  Backout to the job layer and emit flush tokens for all closed-out indices.
    //
    // Example:
    //   Current index: [run: 4, spill: 7]
    //   Received index: nullptr (end of job)
    //   Actions:
    //     a. Emit flush token for [run: 4, spill: 7]
    //     b. Emit flush token for [run: 4]
    //     c. Remove remaining flushes from cache
    if (received_index == nullptr) {
      initialize_index_flush(cached_index);
      // The "std::move" empties the cache, so we don't need to manually clear it after.
      return std::move(pending_flushes_) | std::views::values | std::ranges::to<std::vector>();
    }

    assert(received_index);
    assert(cached_index);

    // A parent must exist at this point
    auto received_parent = received_index->parent();
    assert(received_parent);

    // Received index is immediate child of current index.
    //
    // Example:
    //   Current index: [run: 4]
    //   Received index: [run: 4, spill: 6]
    //   Actions:
    //     a. Initialize count for [run: 4]
    if (received_parent->hash() == cached_index->hash()) {
      create_parent_count(received_parent, received_index);
      return {};
    }

    auto cached_parent = cached_index->parent();

    // Received index is a sibling of the current index.  Increment parent count and move
    // current index to the new sibling.
    //
    // Example:
    //   Current index: [run: 4, spill: 6]
    //   Received index: [run: 4, spill: 7]
    //   Actions:
    //     a. Increment count for [run: 4]
    if (cached_parent and received_parent->hash() == cached_parent->hash()) {
      index_flushes result;
      initialize_index_flush(cached_index);
      auto closed_flush = pending_flushes_.extract(cached_index->hash());
      if (not closed_flush.empty()) {
        result.push_back(std::move(closed_flush.mapped()));
      }
      increment_parent_count(received_parent, received_index);
      return result;
    }

    // Received index is a parent of the cached index.  This means we've closed out the
    // cached index and all of its siblings, and are moving back up the hierarchy.  We need
    // to emit flush tokens for all closed-out indices.
    //
    // Example:
    //   Cached index: [run: 4, spill: 6, subspill: 2, subsubspill: 3]
    //   Received index: [run: 4, spill: 7]
    //   Actions:
    //     a. Emit flush token for [run: 4, spill: 6, subspill: 2]
    //     b. Emit flush token for [run: 4, spill: 6]
    //     c. Remove relevant flush tokens from cache
    //     d. Increment count for [run: 4]
    index_flushes result;
    initialize_index_flush(cached_index);
    auto cached_flush = pending_flushes_.extract(cached_index->hash());
    if (not cached_flush.empty()) {
      result.push_back(std::move(cached_flush.mapped()));
    }
    auto recursive_parent = cached_parent;
    while (recursive_parent != received_parent) {
      if (recursive_parent == nullptr) {
        // We will get here if the received parent is at a layer lower than the cached parent and is
        // not an immediate child of the cached parent. The recursive parent walks all the way back
        // up to the root without finding the received parent. This means the received index is not
        // an ancestor of the cached index, and is invalid.
        throw std::runtime_error(
          fmt::format("Received index {}, which is not an immediate child of {}",
                      received_index->to_string(),
                      cached_index->to_string()));
      }
      initialize_index_flush(recursive_parent);
      auto fh = pending_flushes_.extract(recursive_parent->hash());
      if (not fh.empty()) {
        result.push_back(std::move(fh.mapped()));
      }
      recursive_parent = recursive_parent->parent();
    }
    increment_parent_count(received_parent, received_index);
    return result;
  }

  void data_cell_tracker::set_driver_layer_paths(
    std::vector<experimental::layer_path> const& layer_paths)
  {
    driver_child_layer_hashes_.clear();
    for (auto const& path : layer_paths) {
      auto parent_hash = experimental::identifier{"job"}.hash();
      auto components = path.components();
      if (path.is_complete()) {
        components = components.subspan(1);
      }
      for (auto const& component : components) {
        auto const child_hash = phlex::detail::hash(parent_hash, component.hash());
        driver_child_layer_hashes_[parent_hash].push_back(child_hash);
        parent_hash = child_hash;
      }
    }
    for (auto& child_hashes : driver_child_layer_hashes_ | std::views::values) {
      std::ranges::sort(child_hashes);
      child_hashes.erase(std::unique(child_hashes.begin(), child_hashes.end()), child_hashes.end());
    }
  }

  void data_cell_tracker::initialize_index_flush(data_cell_index_ptr const& index)
  {
    if (pending_flushes_.contains(index->hash())) {
      return;
    }
    auto const it = driver_child_layer_hashes_.find(index->layer_hash());
    if (it == driver_child_layer_hashes_.end()) {
      return;
    }
    // Actual counts already establish completion. Only childless partitions need
    // synthetic zero counts, and only for children owned by the driver.
    auto counts = std::make_shared<data_cell_counts>();
    for (auto const layer_hash : it->second) {
      counts->emplace(layer_hash, 0);
    }
    pending_flushes_.emplace(index->hash(),
                             index_flush{.index = index, .counts = std::move(counts)});
  }

  void data_cell_tracker::create_parent_count(data_cell_index_ptr const& parent,
                                              data_cell_index_ptr const& child)
  {
    pending_flushes_.emplace(parent->hash(),
                             index_flush{.index = parent, .counts = make_data_cell_counts(child)});
  }

  void data_cell_tracker::increment_parent_count(data_cell_index_ptr const& parent,
                                                 data_cell_index_ptr const& child)
  {
    auto it = pending_flushes_.find(parent->hash());
    // The parent's first child has already initialized its count.
    assert(it != pending_flushes_.end());
    it->second.counts->increment(child->layer_hash());
  }
}

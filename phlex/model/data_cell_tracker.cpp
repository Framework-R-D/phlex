#include "phlex/model/data_cell_tracker.hpp"

#include "spdlog/spdlog.h"

#include <cassert>
#include <ranges>
#include <utility>

namespace {
  auto make_data_cell_counts(phlex::data_cell_index_ptr const& index)
  {
    auto result = std::make_shared<phlex::experimental::data_cell_counts>();
    result->emplace(index->layer_hash(), 1);
    return result;
  }
}

namespace phlex::experimental {

  // =========================================================================================
  // data_cell_counts implementation
  void data_cell_counts::emplace(std::size_t layer_hash, std::size_t value)
  {
    map_.emplace(layer_hash, value);
  }

  // =========================================================================================
  // data_cell_tracker implementation
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

  index_flushes data_cell_tracker::closeout(data_cell_index_ptr const& received_index)
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
      increment_parent_count(received_parent, received_index);
      return {};
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
      auto fh = pending_flushes_.extract(recursive_parent->hash());
      result.push_back(fh.mapped());
      recursive_parent = recursive_parent->parent();
    }
    increment_parent_count(received_parent, received_index);
    return result;
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
    // This is only called for siblings, so the parent count must already exist in the cache.
    assert(it != pending_flushes_.end());
    it->second.counts->increment(child->layer_hash());
  }
}

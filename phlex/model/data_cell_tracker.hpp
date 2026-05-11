#ifndef PHLEX_MODEL_DATA_CELL_TRACKER_HPP
#define PHLEX_MODEL_DATA_CELL_TRACKER_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT data_cell_counts {
  public:
    void emplace(std::size_t layer_hash, std::size_t value);

    void increment(data_cell_index::hash_type layer_hash) { ++map_[layer_hash]; }
    void add_to(std::size_t layer_hash, std::size_t value) { map_[layer_hash] += value; }

    auto begin() const { return map_.begin(); }
    auto end() const { return map_.end(); }

    auto size() const { return map_.size(); }

    std::size_t count(data_cell_index::hash_type layer_hash) const
    {
      auto it = map_.find(layer_hash);
      return it != map_.end() ? it->second.load() : 0;
    }

  private:
    tbb::concurrent_unordered_map<data_cell_index::hash_type, std::atomic<std::size_t>> map_;
  };

  using data_cell_counts_ptr = std::shared_ptr<data_cell_counts>;
  using data_cell_counts_const_ptr = std::shared_ptr<data_cell_counts const>;

  struct PHLEX_MODEL_EXPORT index_flush {
    data_cell_index_ptr index;
    // Ideally, the counts field should be a `data_cell_counts_const_ptr` to ensure immutability.
    // However, this type is also used for incrementing counters, so it must be mutable.
    data_cell_counts_ptr counts;
  };

  using index_flushes = std::vector<index_flush>;

  // A simpler flush message sent by an unfold to the index_router.  Unlike index_flush, which
  // carries a map of child counts, unfold_flush carries a single (layer_hash, count) pair
  // because each unfold produces children in exactly one child layer.
  struct PHLEX_MODEL_EXPORT unfold_flush {
    data_cell_index_ptr index;
    data_cell_index::hash_type layer_hash{};
    std::size_t count{};
  };

  // The `closeout_then_emit` struct carries flushes that must be emitted
  // (to close out already-emitted indices) before emitting `index_to_emit`.
  struct PHLEX_MODEL_EXPORT closeout_then_emit {
    index_flushes closeout_flushes{};
    data_cell_index_ptr index_to_emit{nullptr};
  };

  class PHLEX_MODEL_EXPORT data_cell_tracker {
  public:
    data_cell_tracker() = default;
    ~data_cell_tracker();

    data_cell_tracker(data_cell_tracker const&) = delete;
    data_cell_tracker(data_cell_tracker&&) = delete;
    data_cell_tracker& operator=(data_cell_tracker const&) = delete;
    data_cell_tracker& operator=(data_cell_tracker&&) = delete;

    // Computes and returns the set of indices whose processing is now complete, given that
    // the next index to be processed is `index`.  A null `index` signals end-of-job and
    // returns all remaining pending flushes.
    index_flushes closeout(data_cell_index_ptr const& index);

  private:
    void create_parent_count(data_cell_index_ptr const& parent, data_cell_index_ptr const& child);
    void increment_parent_count(data_cell_index_ptr const& parent,
                                data_cell_index_ptr const& child);

    data_cell_index_ptr cached_index_{nullptr};
    std::map<data_cell_index::hash_type, index_flush> pending_flushes_;
  };
}

#endif // PHLEX_MODEL_DATA_CELL_TRACKER_HPP

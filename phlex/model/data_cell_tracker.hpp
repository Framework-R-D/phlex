#ifndef PHLEX_MODEL_DATA_CELL_TRACKER_HPP
#define PHLEX_MODEL_DATA_CELL_TRACKER_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"

#include <map>

namespace phlex::experimental {
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
    index_flushes report_and_evict_ready_flushes(data_cell_index_ptr const& index);

  private:
    void create_parent_count(data_cell_index_ptr const& parent, data_cell_index_ptr const& child);
    void increment_parent_count(data_cell_index_ptr const& parent,
                                data_cell_index_ptr const& child);

    data_cell_index_ptr cached_index_{nullptr};
    std::map<data_cell_index::hash_type, index_flush> pending_flushes_;
  };
}

#endif // PHLEX_MODEL_DATA_CELL_TRACKER_HPP

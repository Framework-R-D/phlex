#ifndef PHLEX_MODEL_DATA_CELL_TRACKER_HPP
#define PHLEX_MODEL_DATA_CELL_TRACKER_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/phlex_model_export.hpp"

#include <initializer_list>
#include <map>
#include <vector>

namespace phlex::detail {
  class PHLEX_MODEL_EXPORT data_cell_tracker {
  public:
    data_cell_tracker(std::initializer_list<experimental::layer_path> layer_paths);
    explicit data_cell_tracker(std::vector<experimental::layer_path> const& layer_paths);
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
    index_flush close_active_family();

    std::map<data_cell_index::hash_type, std::vector<data_cell_index::hash_type>>
      driver_child_layer_hashes_;
    // Each family is identified by its parent index and may span multiple child layers.
    std::vector<index_flush> active_families_;
  };
}

#endif // PHLEX_MODEL_DATA_CELL_TRACKER_HPP

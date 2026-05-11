#ifndef PHLEX_MODEL_CHILD_TRACKER_HPP
#define PHLEX_MODEL_CHILD_TRACKER_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_cell_tracker.hpp"
#include "phlex/phlex_model_export.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>

namespace phlex::experimental {

  class PHLEX_MODEL_EXPORT child_tracker {
    using flush_callback_t = std::function<void(child_tracker const&)>;

  public:
    // expected_flush_count controls how many update_expected_count[s]() calls must arrive before
    // all_children_accounted() can return true.  A value of 0 means a single call is sufficient
    // (the common case when only one unfold consumes this index's layer).  A value greater than 1
    // means that multiple unfolds produce children from the same parent layer.
    explicit child_tracker(data_cell_index_ptr index, std::size_t expected_flush_count);

    data_cell_index_ptr const index() const { return index_; }
    std::size_t expected_total_count() const;
    std::size_t processed_total_count() const;
    std::size_t committed_total_count() const;
    std::size_t committed_count_for_layer(data_cell_index::hash_type layer_hash) const;
    data_cell_counts const& committed_counts() const { return committed_counts_; }

    // Merges expected_counts into the accumulated expected counts.  Each call represents one
    // flush message arriving (e.g. one unfold completing for this index).
    void update_expected_counts(data_cell_counts const& expected_counts);
    // Single-entry variant used when an unfold reports its child count directly (no map needed).
    void update_expected_count(data_cell_index::hash_type layer_hash, std::size_t count);
    void update_committed_counts(data_cell_counts const& committed_counts);
    void increment(data_cell_index::hash_type const layer_hash)
    {
      processed_counts_.increment(layer_hash);
    }

    void set_flush_callback(flush_callback_t callback) { flush_callback_ = std::move(callback); }
    void send_flush();
    bool all_children_accounted();

  private:
    void commit();

    data_cell_index_ptr const index_;
    std::once_flag commit_once_;
    data_cell_counts committed_counts_;
    data_cell_counts processed_counts_;
    // Accumulated expected child counts from all unfolds.
    data_cell_counts expected_counts_;
    std::atomic<std::size_t> received_flush_count_{0};
    // Number of flush messages expected from unfolds.  Zero means any single
    // update_expected_count[s]() call is sufficient to unblock all_children_accounted().
    std::size_t expected_flush_count_{0};
    flush_callback_t flush_callback_;
  };

  using child_tracker_ptr = std::shared_ptr<child_tracker>;

} // namespace phlex::experimental

#endif // PHLEX_MODEL_CHILD_TRACKER_HPP

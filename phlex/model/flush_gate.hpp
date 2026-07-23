#ifndef PHLEX_MODEL_FLUSH_GATE_HPP
#define PHLEX_MODEL_FLUSH_GATE_HPP

// =========================================================================================
// flush_gate
//
// A flush_gate is a per-parent-index completion detector: for a given data_cell_index, it
// decides when the parent's entire subtree of descendant data cells has been accounted for
// and the parent is therefore ready to emit a flush (end-of-subtree token).
//
// One flush_gate is created per data_cell_index by index_router, keyed by the index's hash.
// It accumulates three independent streams of information about that index's subtree:
//
//   1. Expected child counts per child layer, supplied by unfold operations via
//      update_expected_count().  The gate waits for `expected_flush_count` such messages
//      (one per unfold consuming the parent's layer) before treating the expectation as
//      complete.
//
//   2. Rollups of committed counts from non-lowest-layer direct children, recorded via
//      roll_up_child() and balanced against an announced expectation set by
//      expect_child_rollups().  This is a signed counter that must reach zero before the
//      gate opens.
//
//   3. Lowest-layer children, which require no per-arrival accounting: their announced
//      count is the final count and is merged into committed_counts_ at commit time.
//
// Readiness (all_children_accounted()) is a quiescence test: all expected flush messages
// must have arrived AND all expected non-lowest rollups must have been received.  When
// that holds, the gate commits (merges expected_counts_ into committed_counts_) and the
// router walks up to the parent, rolling this gate's counts into the grandparent via
// roll_up_child().
// =========================================================================================

#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/phlex_model_export.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace phlex::detail {

  class PHLEX_MODEL_EXPORT flush_gate {
    using flush_callback_t = std::function<void(flush_gate const&)>;

  public:
    // expected_flush_count controls how many update_expected_count() calls must arrive before
    // all_children_accounted() can return true.  A value of 0 means a single call is sufficient
    // (the common case when only one unfold consumes this index's layer).  A value greater than 1
    // means that multiple unfolds produce children from the same parent layer.
    explicit flush_gate(data_cell_index_ptr index, std::size_t expected_flush_count);

    data_cell_index_ptr const index() const { return index_; }
    std::size_t expected_total_count() const;
    std::size_t committed_total_count() const;
    std::size_t committed_count_for_layer(data_cell_index::hash_type layer_hash) const;
    data_cell_counts const& committed_counts() const { return committed_counts_; }

    // Merges an expected child count into the accumulated expected counts.  Each call
    // represents one flush message arriving (e.g. one unfold completing for this index).
    void update_expected_count(data_cell_index::hash_type layer_hash, std::size_t count);

    // Records that a non-lowest direct child has rolled up: merges its committed_counts
    // into this gate's and decrements the pending-rollups balance.  The two steps are
    // bundled because every rollup must do both, in the same call.
    void roll_up_child(data_cell_counts const& child_committed_counts);

    // Announces that n additional non-lowest direct children are expected to roll up.
    // Lowest-layer children require no such bookkeeping: their counts are fully accounted
    // for by the expected-count message that produced them (from the input_node or an
    // unfold).  The pending counter is signed because rollups can be recorded before the
    // corresponding expected-count message has been processed.
    void expect_child_rollups(std::ptrdiff_t n);

    void set_flush_callback(flush_callback_t callback) { flush_callback_ = std::move(callback); }
    void send_flush();
    bool all_children_accounted();

  private:
    void commit();

    data_cell_index_ptr const index_;
    std::once_flag commit_once_;
    data_cell_counts committed_counts_;
    // Accumulated expected child counts from all unfolds.
    data_cell_counts expected_counts_;
    std::atomic<std::size_t> received_flush_count_{0};
    // Number of flush messages expected from unfolds.  Zero means any single
    // update_expected_count() call is sufficient to unblock all_children_accounted().
    std::size_t expected_flush_count_{0};
    // Signed running balance: (expected non-lowest direct-child rollups) - (rollups received).
    // Commit-ready when this reaches zero (and the expected-count message has arrived).
    std::atomic<std::ptrdiff_t> pending_child_rollups_{0};
    flush_callback_t flush_callback_;
  };

  using flush_gate_ptr = std::shared_ptr<flush_gate>;

} // namespace phlex::detail

#endif // PHLEX_MODEL_FLUSH_GATE_HPP

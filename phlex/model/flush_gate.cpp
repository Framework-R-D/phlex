#include "phlex/model/flush_gate.hpp"

#include "spdlog/spdlog.h"

#include <cassert>
#include <functional>
#include <mutex>
#include <ranges>
#include <utility>

namespace phlex::experimental {

  flush_gate::flush_gate(data_cell_index_ptr index, std::size_t expected_flush_count) :
    index_{std::move(index)},
    committed_counts_{std::make_shared<data_cell_counts>()},
    expected_flush_count_{expected_flush_count}
  {
  }

  std::size_t flush_gate::expected_total_count() const
  {
    return std::ranges::fold_left(expected_counts_ | std::views::values, 0uz, std::plus{});
  }

  std::size_t flush_gate::committed_total_count() const
  {
    return std::ranges::fold_left(*committed_counts_ | std::views::values, 0uz, std::plus{});
  }

  std::size_t flush_gate::committed_count_for_layer(
    data_cell_index::hash_type const layer_hash) const
  {
    return committed_counts_->count(layer_hash);
  }

  void flush_gate::update_expected_count(data_cell_index::hash_type const layer_hash,
                                         std::size_t const count)
  {
    expected_counts_.add_to(layer_hash, count);
    ++received_flush_count_;
  }

  void flush_gate::roll_up_child(data_cell_counts_const_ptr child_committed_counts)
  {
    assert(child_committed_counts);
    for (auto const& [layer_hash, count] : *child_committed_counts) {
      committed_counts_->add_to(layer_hash, count);
    }
    --pending_child_rollups_;
  }

  void flush_gate::expect_child_rollups(std::ptrdiff_t const n) { pending_child_rollups_ += n; }

  void flush_gate::send_flush()
  {
    assert(flush_callback_);
    flush_callback_(*this);
  }

  bool flush_gate::all_children_accounted()
  {
    // Guard against firing before the router has had a chance to set the callback.
    if (not flush_callback_) {
      return false;
    }

    auto const received = received_flush_count_.load();
    if (received == 0) {
      return false;
    }

    // Block until all flush counts expected from unfolds have arrived so that expected_counts_
    // (and the pending_* counters) reflect the union of all child layers.
    if (expected_flush_count_ > 0 and received < expected_flush_count_) {
      return false;
    }

    // Every non-lowest direct child must have rolled up its committed_counts_ into this
    // gate.  Lowest-layer children require no per-arrival accounting: their full count
    // is supplied by the same expected-count message that announced them.
    if (pending_child_rollups_ != 0) {
      return false;
    }

    std::call_once(commit_once_, [this] { commit(); });
    return true;
  }

  void flush_gate::commit()
  {
    for (auto const& [layer_hash, count] : expected_counts_) {
      committed_counts_->add_to(layer_hash, count.load());
    }

    // At some point, we might consider clearing the expected_counts_ map to free memory,
    // but for now we can just leave it as-is since the flush_gate will likely be
    // destroyed soon after commit() is called.
  }

} // namespace phlex::experimental

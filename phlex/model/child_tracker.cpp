#include "phlex/model/child_tracker.hpp"

#include "spdlog/spdlog.h"

#include <functional>
#include <mutex>
#include <ranges>
#include <utility>

namespace phlex::experimental {

  child_tracker::child_tracker(data_cell_index_ptr index, std::size_t expected_flush_count) :
    index_{std::move(index)}, expected_flush_count_{expected_flush_count}
  {
  }

  std::size_t child_tracker::expected_total_count() const
  {
    return std::ranges::fold_left(expected_counts_ | std::views::values, 0uz, std::plus{});
  }

  std::size_t child_tracker::processed_total_count() const
  {
    return std::ranges::fold_left(processed_counts_ | std::views::values, 0uz, std::plus{});
  }

  std::size_t child_tracker::committed_total_count() const
  {
    return std::ranges::fold_left(committed_counts_ | std::views::values, 0uz, std::plus{});
  }

  std::size_t child_tracker::committed_count_for_layer(
    data_cell_index::hash_type const layer_hash) const
  {
    return committed_counts_.count(layer_hash);
  }

  void child_tracker::update_committed_counts(data_cell_counts const& committed_counts)
  {
    for (auto const& [layer_hash, count] : committed_counts) {
      committed_counts_.add_to(layer_hash, count);
    }
  }

  void child_tracker::update_expected_counts(data_cell_counts const& expected_counts)
  {
    for (auto const& [layer_hash, count] : expected_counts) {
      expected_counts_.add_to(layer_hash, count);
    }
    ++received_flush_count_;
  }

  void child_tracker::update_expected_count(data_cell_index::hash_type const layer_hash,
                                            std::size_t const count)
  {
    expected_counts_.add_to(layer_hash, count);
    ++received_flush_count_;
  }

  void child_tracker::send_flush()
  {
    if (flush_callback_) {
      flush_callback_(*this);
    } else {
      spdlog::warn("No flush callback set for index: {}", index_->to_string());
    }
  }

  bool child_tracker::all_children_accounted()
  {
    auto const received = received_flush_count_.load();
    if (received == 0) {
      return false;
    }

    // Block until all flush counts expected from unfolds have arrived so that expected_counts_
    // reflects the union of all child layers.
    if (expected_flush_count_ > 0 and received < expected_flush_count_) {
      return false;
    }

    // All expected flush messages have arrived; check that processed child counts match.
    bool const result = std::ranges::all_of(expected_counts_, [this](auto const& entry) {
      auto const& [layer_hash, expected] = entry;
      return processed_counts_.count(layer_hash) == expected.load();
    });

    if (result) {
      std::call_once(commit_once_, [this] { commit(); });
    }

    return result;
  }

  void child_tracker::commit()
  {
    for (auto const& [layer_hash, count] : processed_counts_) {
      committed_counts_.add_to(layer_hash, count.load());
    }

    // At some point, we might consider clearing the processed_counts_ and expected_counts_ maps
    // to free memory, but for now we can just leave them as-is since the child_tracker will
    // likely be destroyed soon after commit() is called.
  }

} // namespace phlex::experimental

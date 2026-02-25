#include "phlex/core/store_counters.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counter.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <cassert>

namespace phlex::experimental {

  void store_counter::set_flush_value(flush_counts_ptr counts,
                                      std::size_t const original_message_id)
  {
    if (not counts) {
      return;
    }

#ifdef __cpp_lib_atomic_shared_ptr
    flush_counts_ = counts;
#else
    atomic_store(&flush_counts_, counts);
#endif
    original_message_id_ = original_message_id;
  }

  void store_counter::increment(data_cell_index::hash_type const layer_hash)
  {
    ++counts_[layer_hash];
  }

  bool store_counter::is_complete()
  {
    if (!ready_to_flush_) {
      return false;
    }

#ifdef __cpp_lib_atomic_shared_ptr
    auto flush_counts = flush_counts_.load();
#else
    auto flush_counts = atomic_load(&flush_counts_);
#endif
    if (not flush_counts) {
      return false;
    }

    // The 'counts_' data member can be empty if the flush_counts member has been filled
    // but none of the children stores have been processed.
    if (counts_.empty() and !flush_counts->empty()) {
      return false;
    }

    for (auto const& [layer_hash, count] : counts_) {
      auto maybe_count = flush_counts->count_for(layer_hash);
      if (!maybe_count or count != *maybe_count) {
        return false;
      }
    }

    // Flush only once!
    return ready_to_flush_.exchange(false);
  }

  unsigned int store_counter::original_message_id() const noexcept { return original_message_id_; }

  store_counter& count_stores::counter_for(data_cell_index::hash_type const hash)
  {
    counter_accessor ca;
    if (!counters_.find(ca, hash)) {
      counters_.emplace(ca, hash, std::make_unique<store_counter>());
    }
    return *ca->second;
  }

  std::unique_ptr<store_counter> count_stores::done_with(data_cell_index::hash_type const hash)
  {
    // Must be called after an insertion has already been performed
    counter_accessor ca;
    bool const found = counters_.find(ca, hash);
    if (found and ca->second->is_complete()) {
      std::unique_ptr<store_counter> result{std::move(ca->second)};
      counters_.erase(ca);
      return result;
    }
    return nullptr;
  }
}

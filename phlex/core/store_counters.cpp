#include "phlex/core/store_counters.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/level_counter.hpp"

#include "phlex/model/level_id.hpp"
#include <cstddef>
#include <memory>
#include <utility>

namespace phlex::experimental {

  void store_flag::flush_received(std::size_t const original_message_id)
  {
    flush_received_ = true;
    original_message_id_ = original_message_id;
  }

  auto store_flag::is_complete() const noexcept -> bool { return processed_ and flush_received_; }

  void store_flag::mark_as_processed() noexcept { processed_ = true; }

  auto store_flag::original_message_id() const noexcept -> unsigned int { return original_message_id_; }

  auto detect_flush_flag::flag_for(level_id::hash_type const hash) -> store_flag&
  {
    flag_accessor fa;
    flags_.emplace(fa, hash, std::make_unique<store_flag>());
    return *fa->second;
  }

  auto detect_flush_flag::done_with(product_store_const_ptr const& store) -> bool
  {
    auto const h = store->id()->hash();
    if (const_flag_accessor fa; flags_.find(fa, h) && fa->second->is_complete()) {
      flags_.erase(fa);
      return true;
    }
    return false;
  }

  // =====================================================================================

  void store_counter::set_flush_value(product_store_const_ptr const& store,
                                      std::size_t const original_message_id)
  {
    if (not store->contains_product("[flush]")) {
      return;
    }

#ifdef __cpp_lib_atomic_shared_ptr
    flush_counts_ = store->get_product<flush_counts_ptr>("[flush]");
#else
    atomic_store(&flush_counts_, store->get_product<flush_counts_ptr>("[flush]"));
#endif
    original_message_id_ = original_message_id;
  }

  void store_counter::increment(level_id::hash_type const level_hash) { ++counts_[level_hash]; }

  auto store_counter::is_complete() -> bool
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

    for (auto const& [level_hash, count] : counts_) {
      auto maybe_count = flush_counts->count_for(level_hash);
      if (!maybe_count or count != *maybe_count) {
        return false;
      }
    }

    // Flush only once!
    return ready_to_flush_.exchange(false);
  }

  auto store_counter::original_message_id() const noexcept -> unsigned int { return original_message_id_; }

  auto count_stores::counter_for(level_id::hash_type const hash) -> store_counter&
  {
    counter_accessor ca;
    if (!counters_.find(ca, hash)) {
      counters_.emplace(ca, hash, std::make_unique<store_counter>());
    }
    return *ca->second;
  }

  auto count_stores::done_with(level_id::hash_type const hash) -> std::unique_ptr<store_counter>
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

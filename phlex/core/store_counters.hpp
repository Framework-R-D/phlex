#ifndef phlex_core_store_counters_hpp
#define phlex_core_store_counters_hpp

#include "phlex/core/fwd.hpp"
#include "phlex/model/level_counter.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"

#include <atomic>
#include <memory>
#include <version>

namespace phlex::experimental {
  class store_flag {
  public:
    void flush_received(std::size_t original_message_id);
    bool is_complete() const noexcept;
    void mark_as_processed() noexcept;
    unsigned int original_message_id() const noexcept;

  private:
    std::atomic<bool> flush_received_{false};
    std::atomic<bool> processed_{false};
    std::size_t original_message_id_{}; // Necessary for matching inputs to downstream join nodes.
  };

  class detect_flush_flag {
  protected:
    store_flag& flag_for(level_id::hash_type hash);
    bool done_with(product_store_const_ptr const& store);

  private:
    using flags_t = tbb::concurrent_hash_map<level_id::hash_type, std::unique_ptr<store_flag>>;
    using flag_accessor = flags_t::accessor;
    using const_flag_accessor = flags_t::const_accessor;

    flags_t flags_;
  };

  // =========================================================================

  class store_counter {
  public:
    void set_flush_value(product_store_const_ptr const& ptr, std::size_t original_message_id);
    void increment(level_id::hash_type level_hash);
    bool is_complete();
    unsigned int original_message_id() const noexcept;

  private:
    using counts_t2 = tbb::concurrent_unordered_map<level_id::hash_type, std::atomic<std::size_t>>;

    counts_t2 counts_{};
#ifdef __cpp_lib_atomic_shared_ptr
    std::atomic<flush_counts_ptr> flush_counts_{nullptr};
#else
    flush_counts_ptr flush_counts_{nullptr};
#endif
    unsigned int original_message_id_{}; // Necessary for matching inputs to downstream join nodes.
    std::atomic<bool> ready_to_flush_{true};
  };

  class count_stores {
  protected:
    store_counter& counter_for(level_id::hash_type hash);
    std::unique_ptr<store_counter> done_with(level_id::hash_type hash);

  private:
    using counters_t =
      tbb::concurrent_hash_map<level_id::hash_type, std::unique_ptr<store_counter>>;
    using counter_accessor = counters_t::accessor;

    counters_t counters_;
  };
}

#endif // phlex_core_store_counters_hpp

#ifndef PHLEX_CORE_STORE_COUNTERS_HPP
#define PHLEX_CORE_STORE_COUNTERS_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_unordered_map.h"

#include <atomic>
#include <memory>
#include <version>

namespace phlex::experimental {
  class store_counter {
  public:
    void set_flush_value(flush_counts_ptr counts, std::size_t original_message_id);
    void increment(data_cell_index::hash_type layer_hash);
    bool is_complete();
    unsigned int original_message_id() const noexcept;

  private:
    using counts_t2 =
      tbb::concurrent_unordered_map<data_cell_index::hash_type, std::atomic<std::size_t>>;

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
    store_counter& counter_for(data_cell_index::hash_type hash);
    std::unique_ptr<store_counter> done_with(data_cell_index::hash_type hash);

  private:
    using counters_t =
      tbb::concurrent_hash_map<data_cell_index::hash_type, std::unique_ptr<store_counter>>;
    using counter_accessor = counters_t::accessor;

    counters_t counters_;
  };
}

#endif // PHLEX_CORE_STORE_COUNTERS_HPP

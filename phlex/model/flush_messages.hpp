#ifndef PHLEX_MODEL_FLUSH_MESSAGES_HPP
#define PHLEX_MODEL_FLUSH_MESSAGES_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"

#include <cstddef>
#include <vector>

namespace phlex::experimental {
  struct PHLEX_MODEL_EXPORT index_flush {
    data_cell_index_ptr index;
    // Ideally, the counts field should be a `data_cell_counts_const_ptr` to ensure immutability.
    // However, this type is also used for incrementing counters, so it must be mutable.
    data_cell_counts_ptr counts;
  };

  using index_flushes = std::vector<index_flush>;

  // A simpler flush message sent by an unfold to the index_router.  Unlike index_flush, which
  // carries a map of child counts, unfold_flush carries a single (layer_hash, count) pair
  // because each unfold produces children in exactly one child layer.
  struct PHLEX_MODEL_EXPORT unfold_flush {
    data_cell_index_ptr index;
    data_cell_index::hash_type layer_hash{};
    std::size_t count{};
  };

  // The `ready_flushes_then_emit` struct carries flushes that must be emitted
  // (to close out already-emitted indices) before emitting `index_to_emit`.
  struct PHLEX_MODEL_EXPORT ready_flushes_then_emit {
    index_flushes ready_flushes{};
    data_cell_index_ptr index_to_emit{nullptr};
  };
}

#endif // PHLEX_MODEL_FLUSH_MESSAGES_HPP

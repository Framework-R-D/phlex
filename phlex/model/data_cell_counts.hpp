#ifndef PHLEX_MODEL_DATA_CELL_COUNTS_HPP
#define PHLEX_MODEL_DATA_CELL_COUNTS_HPP

#include "phlex/phlex_model_export.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"

#include <atomic>
#include <cstddef>
#include <memory>

namespace phlex::experimental {
  class PHLEX_MODEL_EXPORT data_cell_counts {
  public:
    void emplace(std::size_t layer_hash, std::size_t value);

    void increment(std::size_t layer_hash) { ++map_[layer_hash]; }
    void add_to(std::size_t layer_hash, std::size_t value) { map_[layer_hash] += value; }

    auto begin() const { return map_.begin(); }
    auto end() const { return map_.end(); }

    auto size() const { return map_.size(); }

    std::size_t count(std::size_t layer_hash) const
    {
      auto it = map_.find(layer_hash);
      return it != map_.end() ? it->second.load() : 0;
    }

  private:
    tbb::concurrent_unordered_map<std::size_t, std::atomic<std::size_t>> map_;
  };
}

#endif // PHLEX_MODEL_DATA_CELL_COUNTS_HPP

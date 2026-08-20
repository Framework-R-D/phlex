#include "phlex/model/data_cell_counts.hpp"

namespace phlex::detail {
  void data_cell_counts::emplace(std::size_t layer_hash, std::size_t value)
  {
    map_.emplace(layer_hash, value);
  }
}

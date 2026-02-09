#ifndef PHLEX_MODEL_FWD_HPP
#define PHLEX_MODEL_FWD_HPP

#include <memory>

namespace phlex::experimental {
  class data_cell_counter;
  class data_layer_hierarchy;
  class flush_counts;
  class product_store;

  using flush_counts_ptr = std::shared_ptr<flush_counts const>;
  using product_store_const_ptr = std::shared_ptr<product_store const>;
  using product_store_ptr = std::shared_ptr<product_store>;
}

namespace phlex {
  class data_cell_index;
  using data_cell_index_ptr = std::shared_ptr<data_cell_index const>;
}

#endif // PHLEX_MODEL_FWD_HPP

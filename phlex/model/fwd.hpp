#ifndef PHLEX_MODEL_FWD_HPP
#define PHLEX_MODEL_FWD_HPP

#include <memory>

namespace phlex::experimental {
  class data_cell_counter;
  class data_layer_hierarchy;
  class data_cell_index;
  class product_store;

  using data_cell_index_ptr = std::shared_ptr<data_cell_index const>;
  using product_store_const_ptr = std::shared_ptr<product_store const>;
  using product_store_ptr = std::shared_ptr<product_store>;

  enum class stage { process, flush };
}

#endif // PHLEX_MODEL_FWD_HPP

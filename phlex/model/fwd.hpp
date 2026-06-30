#ifndef PHLEX_MODEL_FWD_HPP
#define PHLEX_MODEL_FWD_HPP

#include <memory>

namespace phlex {
  class data_cell_index;
  using data_cell_index_ptr = std::shared_ptr<data_cell_index const>;

  template <typename T>
  class handle;

  class fixed_hierarchy;
}

namespace phlex::detail {
  template <typename RT>
  class resumable_driver;
}

namespace phlex::detail {
  class data_layer_hierarchy;
  class data_cell_counts;
  using framework_driver = resumable_driver<data_cell_index_ptr>;

  using data_cell_counts_const_ptr = std::shared_ptr<data_cell_counts const>;
  using data_cell_counts_ptr = std::shared_ptr<data_cell_counts>;

}

namespace phlex::experimental {
  class product_store;

  using product_store_const_ptr = std::shared_ptr<product_store const>;
  using product_store_ptr = std::shared_ptr<product_store>;
}

#endif // PHLEX_MODEL_FWD_HPP

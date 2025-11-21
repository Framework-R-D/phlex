#ifndef PHLEX_MODEL_FWD_HPP
#define PHLEX_MODEL_FWD_HPP

#include <memory>

namespace phlex::experimental {
  class level_counter;
  class data_layer_hierarchy;
  class data_cell_id;
  class product_store;

  using data_cell_id_ptr = std::shared_ptr<data_cell_id const>;
  using product_store_const_ptr = std::shared_ptr<product_store const>;
  using product_store_ptr = std::shared_ptr<product_store>;

  enum class stage { process, flush };
}

#endif // PHLEX_MODEL_FWD_HPP

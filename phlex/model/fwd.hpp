#ifndef PHLEX_MODEL_FWD_HPP
#define PHLEX_MODEL_FWD_HPP

#include <cstdint>
#include <memory>

namespace phlex::experimental {
  class level_counter;
  class level_hierarchy;
  class level_id;
  class product_store;

  using level_id_ptr = std::shared_ptr<level_id const>;
  using product_store_const_ptr = std::shared_ptr<product_store const>;
  using product_store_ptr = std::shared_ptr<product_store>;

  enum class stage : std::uint8_t { process, flush };
}

#endif // PHLEX_MODEL_FWD_HPP

#include "phlex/core/declared_transform.hpp"

namespace phlex::detail {
  declared_transform::declared_transform(phlex::experimental::algorithm_name name,
                                         std::vector<std::string> predicates,
                                         product_selectors input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_transform::~declared_transform() = default;
}

#include "phlex/core/declared_fold.hpp"

namespace phlex::experimental {
  declared_fold::declared_fold(algorithm_name name,
                               std::vector<std::string> predicates,
                               product_selectors input_products,
                               std::string partition_layer) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)},
    partition_layer_{std::move(partition_layer)}
  {
  }

  declared_fold::~declared_fold() = default;
}

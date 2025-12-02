#include "phlex/core/declared_fold.hpp"

namespace phlex::experimental {
  declared_fold::declared_fold(algorithm_name name,
                               std::vector<std::string> predicates,
                               product_queries input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_fold::~declared_fold() = default;
}

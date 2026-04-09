#include "phlex/core/declared_fold.hpp"

namespace phlex::experimental {
  declared_fold::declared_fold(algorithm_name name,
                               std::vector<std::string> predicates,
                               product_queries input_products,
                               product_registry const& registry) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products), registry}
  {
  }

  declared_fold::~declared_fold() = default;
}

#include "phlex/core/declared_predicate.hpp"

namespace phlex::experimental {
  declared_predicate::declared_predicate(algorithm_name name,
                                         std::vector<std::string> predicates,
                                         product_queries input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_predicate::~declared_predicate() = default;
}

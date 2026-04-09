#include "phlex/core/declared_observer.hpp"

namespace phlex::experimental {
  declared_observer::declared_observer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products,
                                       product_registry const& registry) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products), registry}
  {
  }

  declared_observer::~declared_observer() = default;
}

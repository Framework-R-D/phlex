#include "phlex/core/declared_observer.hpp"

namespace phlex::experimental {
  declared_observer::declared_observer(algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_queries input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_observer::~declared_observer() = default;
}

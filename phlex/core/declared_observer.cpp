#include "phlex/core/declared_observer.hpp"

namespace phlex::detail {
  declared_observer::declared_observer(phlex::experimental::algorithm_name name,
                                       std::vector<std::string> predicates,
                                       product_selectors input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_observer::~declared_observer() = default;
}

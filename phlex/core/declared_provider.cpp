#include "phlex/core/declared_provider.hpp"

namespace phlex::experimental {
  declared_provider::declared_provider(algorithm_name name, product_queries output_products) :
    products_consumer{std::move(name), {}, std::move(output_products)}
  {
  }

  declared_provider::~declared_provider() = default;
}

#include "phlex/core/declared_provider.hpp"

namespace phlex::experimental {
  declared_provider::declared_provider(algorithm_name name, product_query output_product) :
    name_{std::move(name)}, output_product_{std::move(output_product)}
  {
  }

  declared_provider::~declared_provider() = default;

  std::string declared_provider::full_name() const { return name_.full(); }

  product_query const& declared_provider::output_product() const noexcept
  {
    return output_product_;
  }

  identifier const& declared_provider::layer() const noexcept { return output_product_.layer; }
}

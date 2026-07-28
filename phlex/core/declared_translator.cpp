#include "phlex/core/declared_translator.hpp"

namespace phlex::detail {
  declared_translator::declared_translator(phlex::experimental::algorithm_name name,
                                           std::vector<std::string> predicates,
                                           product_selectors input_products) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)}
  {
  }

  declared_translator::~declared_translator() = default;
}

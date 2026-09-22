#include "phlex/core/declared_predicate.hpp"

namespace phlex::detail {
  declared_predicate::declared_predicate(phlex::experimental::algorithm_name name,
                                         std::vector<std::string> predicates,
                                         product_selectors input_products,
                                         tbb::flow::graph& graph) :
    products_consumer{std::move(name),
                      std::move(predicates),
                      std::move(input_products),
                      graph,
                      require_layers::multi_input_only}
  {
  }

  declared_predicate::~declared_predicate() = default;
}

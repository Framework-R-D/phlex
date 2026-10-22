#include "phlex/core/declared_translator.hpp"

#include "phlex/core/products_consumer.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  declared_translator::declared_translator(phlex::experimental::algorithm_name name,
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

  declared_translator::~declared_translator() = default;
}

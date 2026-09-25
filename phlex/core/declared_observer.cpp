#include "phlex/core/declared_observer.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  declared_observer::declared_observer(phlex::experimental::algorithm_name name,
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

  declared_observer::~declared_observer() = default;
}

#include "phlex/core/declared_transform.hpp"

#include "phlex/core/consumer.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  declared_transform::declared_transform(phlex::experimental::algorithm_name name,
                                         std::vector<std::string> predicates,
                                         product_selectors input_products,
                                         tbb::flow::graph& graph) :
    consumer{std::move(name),
             std::move(predicates),
             std::move(input_products),
             graph,
             require_layers::multi_input_only}
  {
  }

  declared_transform::~declared_transform() = default;
}

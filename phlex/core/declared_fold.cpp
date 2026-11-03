#include "phlex/core/declared_fold.hpp"

#include "phlex/core/consumer.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  declared_fold::declared_fold(phlex::experimental::algorithm_name name,
                               std::vector<std::string> predicates,
                               product_selectors input_products,
                               tbb::flow::graph& graph,
                               std::string partition_layer) :
    consumer{std::move(name),
             std::move(predicates),
             std::move(input_products),
             graph,
             require_layers::always},
    partition_layer_{std::move(partition_layer)}
  {
  }

  declared_fold::~declared_fold() = default;
}

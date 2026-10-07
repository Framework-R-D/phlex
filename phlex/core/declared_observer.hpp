#ifndef PHLEX_CORE_DECLARED_OBSERVER_HPP
#define PHLEX_CORE_DECLARED_OBSERVER_HPP

#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT declared_observer : public products_consumer {
  public:
    declared_observer(phlex::experimental::algorithm_name name,
                      std::vector<std::string> predicates,
                      product_selectors input_products,
                      tbb::flow::graph& graph);
    ~declared_observer() override;
  };

  using declared_observer_ptr = std::unique_ptr<declared_observer>;
  using declared_observers = simple_ptr_map<declared_observer_ptr>;
}

#endif // PHLEX_CORE_DECLARED_OBSERVER_HPP

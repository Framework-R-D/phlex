#ifndef PHLEX_CORE_DECLARED_PREDICATE_HPP
#define PHLEX_CORE_DECLARED_PREDICATE_HPP

#include "phlex/core/consumer.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  struct predicate_result;

  class PHLEX_CORE_EXPORT declared_predicate : public consumer {
  public:
    declared_predicate(phlex::experimental::algorithm_name name,
                       std::vector<std::string> predicates,
                       product_selectors input_products,
                       tbb::flow::graph& graph);
    ~declared_predicate() override;

    virtual tbb::flow::sender<predicate_result>& sender() = 0;
  };

  using declared_predicate_ptr = std::unique_ptr<declared_predicate>;
  using declared_predicates = simple_ptr_map<declared_predicate_ptr>;
}

#endif // PHLEX_CORE_DECLARED_PREDICATE_HPP

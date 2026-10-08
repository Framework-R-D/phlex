#ifndef PHLEX_CORE_DECLARED_FOLD_HPP
#define PHLEX_CORE_DECLARED_FOLD_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT declared_fold : public products_consumer {
  public:
    declared_fold(phlex::experimental::algorithm_name name,
                  std::vector<std::string> predicates,
                  product_selectors input_products,
                  tbb::flow::graph& graph,
                  std::string partition_layer);
    ~declared_fold() override;

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual phlex::experimental::product_specifications const& output() const = 0;
    virtual tbb::flow::receiver<index_message>& partition_port() = 0;
    phlex::experimental::identifier const& partition_layer() const { return partition_layer_; }

  private:
    phlex::experimental::identifier partition_layer_;
  };

  using declared_fold_ptr = std::unique_ptr<declared_fold>;
  using declared_folds = simple_ptr_map<declared_fold_ptr>;
}

#endif // PHLEX_CORE_DECLARED_FOLD_HPP

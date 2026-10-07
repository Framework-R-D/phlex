#ifndef PHLEX_CORE_DECLARED_TRANSFORM_HPP
#define PHLEX_CORE_DECLARED_TRANSFORM_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT declared_transform : public products_consumer {
  public:
    declared_transform(phlex::experimental::algorithm_name name,
                       std::vector<std::string> predicates,
                       product_selectors input_products,
                       tbb::flow::graph& graph);
    ~declared_transform() override;

    virtual tbb::flow::sender<message>& output_port() = 0;
    virtual phlex::experimental::product_specifications const& output() const = 0;
    virtual std::size_t product_count() const = 0;
  };

  using declared_transform_ptr = std::unique_ptr<declared_transform>;
  using declared_transforms = simple_ptr_map<declared_transform_ptr>;
}

#endif // PHLEX_CORE_DECLARED_TRANSFORM_HPP

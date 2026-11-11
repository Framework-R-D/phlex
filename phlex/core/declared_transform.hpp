#ifndef PHLEX_CORE_DECLARED_TRANSFORM_HPP
#define PHLEX_CORE_DECLARED_TRANSFORM_HPP

#include "phlex/core/consumer.hpp"
#include "phlex/core/producer.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT declared_transform : public consumer, public producer {
  public:
    declared_transform(phlex::experimental::algorithm_name name,
                       std::vector<std::string> predicates,
                       product_selectors input_products,
                       tbb::flow::graph& graph);
    ~declared_transform() override;

    phlex::experimental::algorithm_name const& name() const noexcept override
    {
      return consumer::name();
    }
  };

  using declared_transform_ptr = std::unique_ptr<declared_transform>;
  using declared_transforms = simple_ptr_map<declared_transform_ptr>;
}

#endif // PHLEX_CORE_DECLARED_TRANSFORM_HPP

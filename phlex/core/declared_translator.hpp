#ifndef PHLEX_CORE_DECLARED_TRANSLATOR_HPP
#define PHLEX_CORE_DECLARED_TRANSLATOR_HPP

#include "phlex/core/producer.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {

  class PHLEX_CORE_EXPORT declared_translator : public products_consumer, public producer {
  public:
    declared_translator(phlex::experimental::algorithm_name name,
                        std::vector<std::string> predicates,
                        product_selectors input_products,
                        tbb::flow::graph& graph);
    ~declared_translator() override;

    phlex::experimental::algorithm_name const& name() const noexcept override
    {
      return products_consumer::name();
    }

    virtual std::size_t product_count() const = 0;
  };

  using declared_translator_ptr = std::unique_ptr<declared_translator>;
  using declared_translators = simple_ptr_map<declared_translator_ptr>;

}

#endif // PHLEX_CORE_DECLARED_TRANSLATOR_HPP

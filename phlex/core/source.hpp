#ifndef PHLEX_CORE_SOURCE_HPP
#define PHLEX_CORE_SOURCE_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/concurrency.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/index_generator.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <functional>
#include <memory>
#include <string>

namespace phlex::detail {

  // ==============================================================================

  // Function type for type-erased data-product types (used by implicit providers)
  using provider_function_t = product_ptr(data_cell_index const&);

  struct PHLEX_CORE_EXPORT provider_bundle {
    std::function<provider_function_t> provider_function;
    concurrency max_concurrency;
    product_specification spec;
    std::string layer;
    std::string stage;
  };

  using provider_bundles = std::vector<provider_bundle>;

  // ==============================================================================
  class PHLEX_CORE_EXPORT source {
  public:
    virtual ~source() = default;
    virtual provider_bundles create_providers(product_selector const&) = 0;
    virtual index_generator indices() = 0;
  };

  using source_ptr = std::unique_ptr<source>;
  using source_map = simple_ptr_map<source_ptr>;
  using source_vector = std::vector<source const*>;
}

namespace phlex {
  using source = detail::source;
}

#endif // PHLEX_CORE_SOURCE_HPP

#ifndef PHLEX_CORE_SOURCE_HPP
#define PHLEX_CORE_SOURCE_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/index_generator.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"

#include <functional>
#include <memory>

namespace phlex::experimental {

  // ==============================================================================

  // Function type for type-erased data-product types (used by implicit providers)
  using provider_function_t = product_ptr(data_cell_index const&);

  class provider_bundle {
  public:
    provider_bundle(product_specification, provider_function_t);

    product_specification specification() const;
    product_ptr get_product(data_cell_index const&) const;

  private:
    product_specification spec_;
    std::function<provider_function_t> provider_function_;
  };

  using provider_bundles = std::vector<provider_bundle>;

  // ==============================================================================
  class source {
  public:
    virtual ~source() = default;
    // FIXME: Should these functions be 'const'?
    virtual provider_bundles create_providers(product_selector const&) = 0;
    virtual index_generator indices() = 0;
  };

  using source_ptr = std::unique_ptr<source>;
}

#endif // PHLEX_CORE_SOURCE_HPP

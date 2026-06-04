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

namespace phlex::experimental {

  // ==============================================================================

  // Function type for type-erased data-product types (used by implicit providers)
  using provider_function_t = product_ptr(data_cell_index const&);

  class provider_bundle {
  public:
    provider_bundle(provider_function_t f,
                    concurrency c,
                    product_specification spec,
                    std::string layer,
                    std::string stage);

    product_specification const& specification() const;
    identifier const& layer() const;
    std::function<provider_function_t> release_provider_function();
    concurrency get_concurrency() const;

  private:
    std::function<provider_function_t> provider_function_;
    concurrency concurrency_;
    product_specification spec_;
    identifier layer_;
    identifier stage_;
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
  using source_map = simple_ptr_map<source_ptr>;
}

#endif // PHLEX_CORE_SOURCE_HPP

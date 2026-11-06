#ifndef PHLEX_CORE_SOURCE_HPP
#define PHLEX_CORE_SOURCE_HPP

#include "phlex/core/product_selector.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/index_generator.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <functional>
#include <memory>
#include <string>

namespace phlex::detail {

  /// @brief Base class for a factory of providers, which supply data products on demand.
  ///
  /// Register a source with @c add_source<T>(name, args...) in a @c PHLEX_REGISTER_SOURCE
  /// entry point.  For each algorithm input that no algorithm or explicit provider creates,
  /// the framework asks every source for providers with @c create_providers().
  class PHLEX_CORE_EXPORT source {
  public:
    virtual ~source() = default;

    /// @brief Returns providers for the products this source can supply that match the selector.
    ///
    /// Return an empty vector if the source has no matching product.  Any of the selector's
    /// fields may be unset; check a candidate with
    /// @c product_selector::match(spec, layer, stage) rather than reading the fields.
    /// Each returned bundle becomes a separate provider node, and its @c stage must not be
    /// @c "CURRENT".  Across all sources, at most one bundle may match a selector.
    virtual provider_bundles create_providers(product_selector const&) = 0;

    /// @brief Yields the data cells this source holds.
    ///
    /// The framework does not call this function; a driver that lists the source in its
    /// @c uses_sources configuration may.  The default yields nothing.
    // Clang-tidy misdiagnoses the coroutine's generated promise_type access.
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    virtual index_generator indices() { co_return; }
  };

  using source_ptr = std::unique_ptr<source>;
  using source_map = simple_ptr_map<source_ptr>;
  using source_vector = std::vector<source const*>;
}

namespace phlex {
  using source = detail::source;
}

#endif // PHLEX_CORE_SOURCE_HPP

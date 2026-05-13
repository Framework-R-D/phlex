#ifndef PHLEX_DRIVER_HPP
#define PHLEX_DRIVER_HPP

#include "phlex/configuration.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/detail/plugin_macros.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/async_driver.hpp"

#include <concepts>
#include <functional>
#include <utility>

namespace phlex::experimental {
  class driver_proxy;
  struct driver_bundle;

  using framework_driver = experimental::async_driver<data_cell_index_ptr>;

  namespace detail {
    using next_index_t = std::function<void(framework_driver&)>;
    using driver_creator_t = driver_bundle(driver_proxy const&, configuration const&);
    // Shim type for the extern "C" entry-point: out-parameter avoids returning a C++ type
    // across a C-linkage boundary.
    using driver_shim_t = void(driver_proxy const&, configuration const&, driver_bundle*);
  };

  /// @brief Bundles the driver function and data hierarchy for the framework.
  struct driver_bundle {
    detail::next_index_t driver; ///< Driver function that advances data cells.
    fixed_hierarchy hierarchy;   ///< Data hierarchy traversed by the driver.
  };
}

namespace phlex::experimental {
  template <typename F>
  concept is_driver_like = std::invocable<F, data_cell_cursor const&>;

  /// @brief Proxy for constructing a driver bundle from a user-supplied driver function.
  ///
  /// Passed to @c PHLEX_REGISTER_DRIVER plugin entry points. Users never
  /// construct this type directly.
  class driver_proxy {
  public:
    /// @brief Creates a driver_bundle from a hierarchy and a user-supplied driver function.
    ///
    /// @param hierarchy  The data hierarchy the driver will traverse.
    /// @param driver_function  A callable receiving a @c data_cell_cursor const& that
    ///                         emits data cells to the framework.
    driver_bundle driver(fixed_hierarchy hierarchy, is_driver_like auto driver_function) const
    {
      auto h = hierarchy;
      return {[f = std::move(driver_function), h = std::move(h)](framework_driver& d) mutable {
                f(h.yield_job(d));
              },
              std::move(hierarchy)};
    }
  };
}

#define PHLEX_REGISTER_DRIVER(...)                                                                 \
  PHLEX_DETAIL_REGISTER_DRIVER_PLUGIN(create, create_driver, __VA_ARGS__)

#endif // PHLEX_DRIVER_HPP

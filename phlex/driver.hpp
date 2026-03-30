#ifndef PHLEX_DRIVER_HPP
#define PHLEX_DRIVER_HPP

#include "boost/dll/alias.hpp"

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
  };

  struct driver_bundle {
    detail::next_index_t driver;
    fixed_hierarchy hierarchy;
  };
}

namespace phlex::experimental {
  template <typename F>
  concept is_driver_like = std::invocable<F, data_cell_cursor const&>;

  class driver_proxy {
  public:
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

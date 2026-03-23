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

#include <cassert>
#include <functional>

namespace phlex {
  using framework_driver = experimental::async_driver<data_cell_index_ptr>;
}

namespace phlex::experimental {
  class driver_proxy;
}

namespace phlex::experimental::detail {
  using next_index_t = std::function<void(framework_driver&)>;
  using validator_t = std::function<void(data_cell_index_ptr const&)>;
  using driver_creator_t = void(driver_proxy&, configuration const&);
}

namespace phlex::experimental {
  class driver_proxy {
  public:
    void driver(fixed_hierarchy hierarchy, detail::next_index_t f)
    {
      hierarchy_ = std::move(hierarchy);
      driver_ = std::move(f);
    }

    fixed_hierarchy release_hierarchy() { return std::move(hierarchy_); }

    detail::next_index_t release()
    {
      assert(driver_ && "No driver has been registered via driver()");
      return std::move(driver_);
    }

  private:
    fixed_hierarchy hierarchy_;
    detail::next_index_t driver_;
  };
}

namespace phlex::experimental::detail {
  struct driver_with_validator {
    next_index_t driver;
    fixed_hierarchy hierarchy;
  };
}

#define PHLEX_EXPERIMENTAL_REGISTER_DRIVER(...)                                                    \
  PHLEX_DETAIL_REGISTER_DRIVER_PLUGIN(create, create_driver, __VA_ARGS__)

#endif // PHLEX_DRIVER_HPP

#ifndef PHLEX_DRIVER_HPP
#define PHLEX_DRIVER_HPP

#include "boost/dll/alias.hpp"

#include "phlex/configuration.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/detail/plugin_macros.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/async_driver.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/format.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace phlex {
  using framework_driver = experimental::async_driver<data_cell_index_ptr>;
}

namespace phlex::experimental {
  class driver_proxy;
}

namespace phlex::experimental::detail {
  using next_index_t = std::function<void(framework_driver&)>;
  using validator_t = std::function<void(data_cell_index const&)>;
  using driver_creator_t = void(driver_proxy&, configuration const&);
}

namespace phlex::experimental {
  class fixed_hierarchy {
  public:
    fixed_hierarchy() = default;

    explicit fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths)
    {
      identifier const job{"job"};
      std::set<std::size_t> hashes{job.hash()};
      for (std::vector<std::string> const& path : layer_paths) {
        bool const has_job_prefix = !path.empty() && path[0] == "job";
        std::size_t cumulative_hash = job.hash();
        std::size_t i_start = 0;
        if (has_job_prefix) {
          i_start = 1;
        }
        for (std::size_t i = i_start; i != path.size(); ++i) {
          identifier const layer{path[i]};
          cumulative_hash = hash(cumulative_hash, layer.hash());
          hashes.insert(cumulative_hash);
        }
      }
      layer_hashes_.assign(hashes.begin(), hashes.end());
    }

    auto validator() const
    {
      return [this](data_cell_index const& index) { validate(index); };
    }

  private:
    void validate(data_cell_index const& index) const
    {
      if (layer_hashes_.empty()) {
        return;
      }
      if (std::ranges::binary_search(layer_hashes_, index.layer_hash())) {
        return;
      }
      throw std::runtime_error(
        fmt::format("Layer {} is not part of the fixed hierarchy.", index.to_string()));
    }

    std::vector<std::size_t> layer_hashes_;
  };

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

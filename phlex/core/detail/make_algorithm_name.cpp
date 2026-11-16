#include "phlex/core/detail/make_algorithm_name.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace phlex::experimental::internal {
  algorithm_name make_algorithm_name(configuration const* config, std::string_view name)
  {
    std::optional<identifier> plugin;
    if (config) {
      plugin.emplace(config->get<std::string_view>("module_label"));
    }
    return {std::move(plugin), identifier(name)};
  }
}

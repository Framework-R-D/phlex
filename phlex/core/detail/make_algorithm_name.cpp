#include "phlex/core/detail/make_algorithm_name.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <optional>
#include <string_view>

namespace phlex::experimental::internal {
  algorithm_name make_algorithm_name(configuration const* config, std::string_view name)
  {
    std::optional<std::string_view> plugin_label;
    if (config) {
      plugin_label = config->get<std::string_view>("module_label");
    }
    return algorithm_name::create(plugin_label, name);
  }
}

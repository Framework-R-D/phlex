#include "phlex/core/detail/make_algorithm_name.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <optional>
#include <string_view>

namespace phlex::experimental::internal {
  // The plugin component is absent only when there is no configuration. A
  // configuration must supply a non-empty 'module_label': a missing label is an error reported by
  // the configuration, and an empty label is rejected by algorithm_name::create rather than
  // treated as an absent plugin.
  algorithm_name make_algorithm_name(configuration const* config, std::string_view name)
  {
    std::optional<std::string_view> plugin_label;
    if (config) {
      plugin_label = config->get<std::string_view>("module_label");
    }
    return algorithm_name::create(plugin_label, name);
  }
}

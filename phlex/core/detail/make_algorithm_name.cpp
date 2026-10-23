#include "phlex/core/detail/make_algorithm_name.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"

#include <stdexcept>
#include <string_view>

namespace phlex::experimental::internal {
  // The plugin component is absent (explicitly empty) only when there is no configuration. A
  // configuration must supply a non-empty 'module_label': a missing label is an error reported by
  // the configuration, and an empty label is rejected here rather than treated as an absent
  // plugin.
  algorithm_name make_algorithm_name(configuration const* config, std::string_view name)
  {
    if (config == nullptr) {
      return {identifier_or_empty{empty_string{}}, identifier(name)};
    }
    auto const module_label = config->get<std::string_view>("module_label");
    if (module_label.empty()) {
      throw std::runtime_error("The configured 'module_label' must not be empty");
    }
    return {identifier_or_empty{identifier(module_label)}, identifier(name)};
  }
}

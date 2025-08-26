#include "phlex/core/glue.hpp"
#include "phlex/configuration.hpp"

#include <string>

namespace phlex::experimental::detail {
  void verify_name(std::string const& name, configuration const* config)
  {
    if (not name.empty()) {
      return;
    }

    std::string msg{"Cannot specify algorithm with no name"};
    std::string const module = config ? config->get<std::string>("module_label") : "";
    if (!module.empty()) {
      msg += " (module: '";
      msg += module;
      msg += "')";
    }
    throw std::runtime_error{msg};
  }
}

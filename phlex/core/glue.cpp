#include "phlex/core/glue.hpp"

#include "phlex/configuration.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/source.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex::detail::internal {
  void register_source(source_map& sources,
                       std::vector<std::string>& errors,
                       std::string_view name,
                       source_ptr src)
  {
    auto [_, inserted] = sources.try_emplace(std::string{name}, std::move(src));
    if (not inserted) {
      add_to_error_messages(errors, "Source", std::string{name});
    }
  }

  void verify_name(std::string_view name, configuration const* config)
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

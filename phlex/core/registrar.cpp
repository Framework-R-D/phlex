#include "phlex/core/registrar.hpp"

#include <fmt/core.h>
#include <string>
#include <vector>

namespace phlex::experimental::detail {
  void add_to_error_messages(std::vector<std::string>& errors, std::string const& name)
  {
    errors.push_back(fmt::format("Node with name '{}' already exists", name));
  }
}

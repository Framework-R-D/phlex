#include "phlex/core/registrar.hpp"

#include "fmt/format.h"

namespace phlex::experimental::detail {
  void add_to_error_messages(std::vector<std::string>& errors,
                             std::string const& entity,
                             std::string const& name)
  {
    errors.push_back(fmt::format("{} with name '{}' already exists", entity, name));
  }
}

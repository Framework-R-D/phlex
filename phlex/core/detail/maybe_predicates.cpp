#include "phlex/core/detail/maybe_predicates.hpp"
#include "phlex/configuration.hpp"
#include <optional>
#include <string>
#include <vector>

namespace phlex::experimental::detail {
  auto maybe_predicates(configuration const* config) -> std::optional<std::vector<std::string>>
  {
    return config->get_if_present<std::vector<std::string>>("when");
  }
}

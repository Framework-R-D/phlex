#include "phlex/core/detail/maybe_predicates.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"

namespace phlex::experimental::detail {
  std::optional<std::vector<std::string>> maybe_predicates(configuration const* config)
  {
    return config->get_if_present<std::vector<std::string>>("when");
  }

  algorithm_name make_algorithm_name(configuration const* config, std::string name)
  {
    return {config ? config->get<std::string>("module_label") : "", std::move(name)};
  }
}

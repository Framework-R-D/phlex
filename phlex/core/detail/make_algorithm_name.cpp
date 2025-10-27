#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"
#include <string>
#include <utility>

namespace phlex::experimental::detail {
  auto make_algorithm_name(configuration const* config, std::string name) -> algorithm_name
  {
    return {config ? config->get<std::string>("module_label") : "", std::move(name)};
  }
}

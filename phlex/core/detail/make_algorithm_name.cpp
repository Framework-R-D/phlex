#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"

namespace phlex::experimental::detail {
  algorithm_name make_algorithm_name(configuration const* config, std::string name)
  {
    return {config ? config->get<std::string>("module_label") : "", std::move(name)};
  }
}

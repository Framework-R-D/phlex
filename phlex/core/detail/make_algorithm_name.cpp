#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/configuration.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"

namespace phlex::experimental::detail {
  algorithm_name make_algorithm_name(configuration const* config, std::string_view name)
  {
    return {config ? identifier(config->get<std::string_view>("module_label")) : ""_id, identifier(name)};
  }
}

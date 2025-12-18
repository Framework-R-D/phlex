#ifndef PHLEX_CORE_DETAIL_MAYBE_PREDICATES_HPP
#define PHLEX_CORE_DETAIL_MAYBE_PREDICATES_HPP

// This simple utility is placed in an implementation file to avoid including the
// phlex/configuration.hpp in framework code.

#include <optional>
#include <string>
#include <vector>

namespace phlex {
  class configuration;
}

namespace phlex::experimental::detail {
  std::optional<std::vector<std::string>> maybe_predicates(configuration const* config);
}

#endif // PHLEX_CORE_DETAIL_MAYBE_PREDICATES_HPP

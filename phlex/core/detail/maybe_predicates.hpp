#ifndef phlex_core_detail_maybe_predicates_hpp
#define phlex_core_detail_maybe_predicates_hpp

// This simple utility is placed in an implementation file to avoid including the
// phlex/configuration.hpp in framework code.

#include <optional>
#include <string>
#include <vector>

namespace phlex::experimental {
  class configuration;

  namespace detail {
    std::optional<std::vector<std::string>> maybe_predicates(configuration const* config);
  }
}

#endif

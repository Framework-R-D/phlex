#ifndef PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP
#define PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP

// This simple utility is placed in an implementation file to avoid including the
// phlex/configuration.hpp in framework code.

#include <string>

#include "phlex_core_export.hpp"

namespace phlex {
  class configuration;
}

namespace phlex::experimental {
  class algorithm_name;

  namespace detail {
    phlex_core_EXPORT algorithm_name make_algorithm_name(configuration const* config,
                                                         std::string name);
  }
}

#endif // PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP

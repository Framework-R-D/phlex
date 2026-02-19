#ifndef PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP
#define PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP

// This simple utility is placed in an implementation file to avoid including the
// phlex/configuration.hpp in framework code.

#include <string_view>

namespace phlex {
  class configuration;
}

namespace phlex::experimental {
  class algorithm_name;

  namespace detail {
    algorithm_name make_algorithm_name(configuration const* config, std::string_view name);
  }
}

#endif // PHLEX_CORE_DETAIL_MAKE_ALGORITHM_NAME_HPP

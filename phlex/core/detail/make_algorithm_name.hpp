#ifndef phlex_core_detail_make_algorithm_name_hpp
#define phlex_core_detail_make_algorithm_name_hpp

// This simple utility is placed in an implementation file to avoid including the
// phlex/configuration.hpp in framework code.

#include <string>

namespace phlex::experimental {
  class algorithm_name;
  class configuration;

  namespace detail {
    algorithm_name make_algorithm_name(configuration const* config, std::string name);
  }
}

#endif

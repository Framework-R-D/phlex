#ifndef phlex_utilities_max_allowed_parallelism_hpp
#define phlex_utilities_max_allowed_parallelism_hpp

#include "oneapi/tbb/global_control.h"

#include <cstddef>

namespace phlex::experimental {
  class max_allowed_parallelism {
  public:
    explicit max_allowed_parallelism(std::size_t const n) :
      control_{tbb::global_control::max_allowed_parallelism, n}
    {
    }

    static auto active_value()
    {
      using control = tbb::global_control;
      return control::active_value(control::max_allowed_parallelism);
    }

  private:
    tbb::global_control control_;
  };
}

#endif // phlex_utilities_max_allowed_parallelism_hpp

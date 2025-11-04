#ifndef PHLEX_UTILITIES_MAX_ALLOWED_PARALLELISM_HPP
#define PHLEX_UTILITIES_MAX_ALLOWED_PARALLELISM_HPP

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

#endif // PHLEX_UTILITIES_MAX_ALLOWED_PARALLELISM_HPP

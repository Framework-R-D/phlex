#ifndef PHLEX_UTILITIES_RESOURCE_USAGE_HPP
#define PHLEX_UTILITIES_RESOURCE_USAGE_HPP

// =======================================================================================
// The resource_usage class tracks the CPU time and real time during the lifetime of a
// resource_usage object.  The destructor will also report the maximum RSS of the process.
// =======================================================================================

#include "phlex_utilities_export.hpp"

#include <chrono>

namespace phlex::experimental {
  class phlex_utilities_EXPORT resource_usage {
  public:
    resource_usage() noexcept;
    ~resource_usage();

  private:
    std::chrono::time_point<std::chrono::steady_clock> begin_wall_;
    double begin_cpu_;
  };
}

#endif // PHLEX_UTILITIES_RESOURCE_USAGE_HPP

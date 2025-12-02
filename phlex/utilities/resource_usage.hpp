#ifndef PHLEX_UTILITIES_RESOURCE_USAGE_HPP
#define PHLEX_UTILITIES_RESOURCE_USAGE_HPP

// =======================================================================================
// The resource_usage class tracks the CPU time and real time during the lifetime of a
// resource_usage object.  The destructor will also report the maximum RSS of the process.
// =======================================================================================

#include <chrono>

namespace phlex::experimental {
  class resource_usage {
  public:
    resource_usage() noexcept;
    ~resource_usage();

    resource_usage(resource_usage const&);
    resource_usage& operator=(resource_usage const&);

    // NOLINTBEGIN(cppcoreguidelines-noexcept-move-operations,performance-noexcept-move-constructor)
    resource_usage(resource_usage&&);
    resource_usage& operator=(resource_usage&&);
    // NOLINTEND(cppcoreguidelines-noexcept-move-operations,performance-noexcept-move-constructor)

  private:
    std::chrono::time_point<std::chrono::steady_clock> begin_wall_;
    double begin_cpu_;
  };
}

#endif // PHLEX_UTILITIES_RESOURCE_USAGE_HPP

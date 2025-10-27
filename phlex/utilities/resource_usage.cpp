#include "phlex/utilities/resource_usage.hpp"

#include "spdlog/spdlog.h"

#include <bits/types/struct_rusage.h>
#include <chrono>
#include <sys/resource.h>

using namespace std::chrono;

#if __linux__
constexpr double mem_denominator{1e3};
#else // AppleClang
constexpr double mem_denominator{1e6};
#endif

namespace {
  struct metrics {
    double elapsed_time;
    double max_rss;
  };

  auto get_rusage() noexcept -> metrics
  {
    rusage used;
    getrusage(RUSAGE_SELF, &used);
    auto const [secs, microsecs] = used.ru_utime;
    return {.elapsed_time = secs + microsecs / 1e6, .max_rss = used.ru_maxrss / mem_denominator};
  }
}

namespace phlex::experimental {
  resource_usage::resource_usage() noexcept :
    begin_wall_{steady_clock::now()}, begin_cpu_{get_rusage().elapsed_time}
  {
  }

  resource_usage::~resource_usage()
  {
    auto const [elapsed_time, max_rss] = get_rusage();
    auto const end_wall = steady_clock::now();
    auto const cpu_time = elapsed_time - begin_cpu_;
    auto const real_time = duration_cast<nanoseconds>(end_wall - begin_wall_).count() / 1e9;
    spdlog::info("CPU time: {:.5f}s  Real time: {:.5f}s  CPU efficiency: {:6.2f}%",
                 cpu_time,
                 real_time,
                 cpu_time / real_time * 100);
    spdlog::info("Max. RSS: {:.3f} MB", max_rss);
  }
}

// ===================================================================
// This source creates 1M events.
// ===================================================================

#include "phlex/source.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

#include <ranges>

namespace test {
  class benchmarks_source {
  public:
    benchmarks_source(phlex::experimental::configuration const& config) :
      max_{config.get<std::size_t>("n_events")}
    {
      spdlog::info("Processing {} events", max_);
    }

    void next(phlex::experimental::framework_driver& driver) const
    {
      auto job_index = phlex::experimental::data_cell_index::base_ptr();
      driver.yield(job_index);

      for (std::size_t i : std::views::iota(0u, max_)) {
        if (max_ > 10 and i % (max_ / 10) == 0) {
          spdlog::debug("Reached {} events", i);
        }

        auto index = job_index->make_child(i, "event");
        driver.yield(index);
      }
    }

  private:
    std::size_t max_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(test::benchmarks_source)

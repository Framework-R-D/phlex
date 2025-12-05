#ifndef TEST_CACHED_EXECUTION_SOURCE_HPP
#define TEST_CACHED_EXECUTION_SOURCE_HPP

// ===================================================================
// This source creates:
//
//  1 run
//    2 subruns per run
//      5000 events per subrun
// ===================================================================

#include "phlex/source.hpp"

#include <ranges>

namespace test {
  inline constexpr std::size_t n_runs{1};
  inline constexpr std::size_t n_subruns{2u};
  inline constexpr std::size_t n_events{5000u};

  class cached_execution_source {
  public:
    void next(phlex::experimental::framework_driver& driver)
    {
      using namespace phlex::experimental;

      auto job_store = product_store::base();
      driver.yield(job_store);

      for (std::size_t i : std::views::iota(0u, n_runs)) {
        auto run_store = job_store->make_child(i, "run");
        driver.yield(run_store);

        for (std::size_t j : std::views::iota(0u, n_subruns)) {
          auto subrun_store = run_store->make_child(j, "subrun");
          driver.yield(subrun_store);

          for (std::size_t k : std::views::iota(0u, n_events)) {
            auto event_store = subrun_store->make_child(k, "event");
            driver.yield(event_store);
          }
        }
      }
    }
  };
}

#endif // TEST_CACHED_EXECUTION_SOURCE_HPP

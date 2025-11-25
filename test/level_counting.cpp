#include "phlex/model/level_counter.hpp"
#include "phlex/model/level_hierarchy.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/hashing.hpp"

#include "catch2/catch_test_macros.hpp"
#include <cstddef>

using namespace phlex::experimental;

namespace {
  auto const job_hash_value = hash("job");
}

TEST_CASE("Counter with nothing processed", "[data model]")
{
  level_counter job_counter{};
  CHECK(job_counter.result().empty());
}

TEST_CASE("Counter one layer deep", "[data model]")
{
  level_counter job_counter{};
  for (std::size_t i = 0; i != 10; ++i) {
    job_counter.make_child("event");
  }
  auto const event_hash_value = hash(job_hash_value, "event");
  CHECK(job_counter.result().count_for(event_hash_value) == 10);
}

TEST_CASE("Counter multiple layers deep", "[data model]")
{
  constexpr std::size_t nruns{2ull};
  constexpr std::size_t nsubruns_per_run{3ull};
  constexpr std::size_t nevents_per_subrun{5ull};

  std::size_t processed_jobs{};
  std::size_t processed_runs{};
  std::size_t processed_subruns{};
  std::size_t processed_events{};

  level_hierarchy h;
  flush_counters counters;

  // Notice the wholesale capture by reference--generally a lazy way of doing things.
  auto check_all_processed = [&] -> void {
    CHECK(h.count_for("job") == processed_jobs);
    CHECK(h.count_for("run") == processed_runs);
    CHECK(h.count_for("subrun") == processed_subruns);
    CHECK(h.count_for("event") == processed_events);
  };

  auto const run_hash_value = hash(job_hash_value, "run");
  auto const subrun_hash_value = hash(run_hash_value, "subrun");
  auto const event_hash_value = hash(subrun_hash_value, "event");

  auto job_store = product_store::base();
  counters.update(job_store->id());
  for (std::size_t i = 0; i != nruns; ++i) {
    auto run_store = job_store->make_child(i, "run");
    counters.update(run_store->id());
    for (std::size_t j = 0; j != nsubruns_per_run; ++j) {
      auto subrun_store = run_store->make_child(j, "subrun");
      counters.update(subrun_store->id());
      for (std::size_t k = 0; k != nevents_per_subrun; ++k) {
        auto event_store = subrun_store->make_child(k, "event");
        counters.update(event_store->id());
        ++processed_events;

        h.increment_count(event_store->id());
        auto results = counters.extract(event_store->id());
        CHECK(results.empty());
        check_all_processed();
      }
      h.increment_count(subrun_store->id());
      auto results = counters.extract(subrun_store->id());
      ++processed_subruns;

      CHECK(results.count_for(event_hash_value));
      check_all_processed();
    }
    h.increment_count(run_store->id());
    auto results = counters.extract(run_store->id());
    ++processed_runs;

    CHECK(results.count_for(event_hash_value) == nevents_per_subrun * nsubruns_per_run);
    CHECK(results.count_for(subrun_hash_value) == nsubruns_per_run);
    check_all_processed();
  }
  h.increment_count(job_store->id());
  auto results = counters.extract(job_store->id());
  ++processed_jobs;

  CHECK(results.count_for(event_hash_value) == nevents_per_subrun * nsubruns_per_run * nruns);
  CHECK(results.count_for(subrun_hash_value) == nsubruns_per_run * nruns);
  CHECK(results.count_for(run_hash_value) == nruns);
  check_all_processed();
}

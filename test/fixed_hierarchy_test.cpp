#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fixed_hierarchy.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;
using namespace phlex::experimental;
using namespace Catch::Matchers;

// Builds indices for the hierarchy from the issue description:
//
//   run
//    │
//    ├ subrun
//    │  │
//    │  └ spill
//    │
//    └ calibration_period
//
// Specified as: {{"run", "subrun", "spill"}, {"run", "calibration_period"}}

namespace {
  auto make_hierarchy()
  {
    return fixed_hierarchy{{"run", "subrun", "spill"}, {"run", "calibration_period"}};
  }
}

TEST_CASE("Ill-formed paths result in an exception", "[fixed_hierarchy]")
{
  CHECK_THROWS_WITH(fixed_hierarchy{{}}, ContainsSubstring("Layer paths cannot be empty"));
  CHECK_THROWS_WITH((fixed_hierarchy{{"job", "run", "job"}}),
                    ContainsSubstring("Layer paths may only contain 'job' as the first element"));
}

TEST_CASE("Default-constructed fixed_hierarchy accepts any index", "[fixed_hierarchy]")
{
  fixed_hierarchy const h;
  auto const job = data_cell_index::job();
  CHECK_NOTHROW(h.validate(job));
  CHECK_NOTHROW(h.validate(job->make_child("run", 0)));
  CHECK_NOTHROW(h.validate(job->make_child("unrelated", 0)));
}

TEST_CASE("Fixed hierarchy accepts valid layer indices", "[fixed_hierarchy]")
{
  auto const h = make_hierarchy();
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const subrun = run->make_child("subrun", 0);

  CHECK_NOTHROW(h.validate(job));
  CHECK_NOTHROW(h.validate(run));
  CHECK_NOTHROW(h.validate(subrun));
  CHECK_NOTHROW(h.validate(subrun->make_child("spill", 0)));
  CHECK_NOTHROW(h.validate(run->make_child("calibration_period", 0)));
}

TEST_CASE("Fixed hierarchy rejects indices not present in the hierarchy", "[fixed_hierarchy]")
{
  auto const h = make_hierarchy();
  auto const job = data_cell_index::job();

  // Layer not declared in the hierarchy at all
  CHECK_THROWS_WITH(h.validate(job->make_child("unknown", 0)),
                    ContainsSubstring("Layer /job/unknown is not part of the fixed hierarchy"));

  // "subrun" exists but only under "run", not directly under "job"
  CHECK_THROWS_WITH(h.validate(job->make_child("subrun", 0)),
                    ContainsSubstring("Layer /job/subrun is not part of the fixed hierarchy"));
}

TEST_CASE("Paths with and without 'job' prefix produce the same hierarchy", "[fixed_hierarchy]")
{
  fixed_hierarchy const without_prefix{{"run", "subrun"}, {"run", "calibration_period"}};
  fixed_hierarchy const with_prefix{{"job", "run", "subrun"}, {"job", "run", "calibration_period"}};

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const subrun = run->make_child("subrun", 0);
  auto const cal_period = run->make_child("calibration_period", 0);
  auto const unknown = job->make_child("unknown", 0);

  for (auto const* h : {&without_prefix, &with_prefix}) {
    CHECK_NOTHROW(h->validate(job));
    CHECK_NOTHROW(h->validate(run));
    CHECK_NOTHROW(h->validate(subrun));
    CHECK_NOTHROW(h->validate(cal_period));
    CHECK_THROWS_WITH(h->validate(unknown),
                      ContainsSubstring("Layer /job/unknown is not part of the fixed hierarchy"));
  }
}

TEST_CASE("validator() callable behaves identically to validate()", "[fixed_hierarchy]")
{
  auto const h = make_hierarchy();
  auto const validate = h.validator();

  auto const job = data_cell_index::job();
  CHECK_NOTHROW(validate(job));
  CHECK_NOTHROW(validate(job->make_child("run", 0)));
  CHECK_THROWS_WITH(validate(job->make_child("unknown", 0)),
                    ContainsSubstring("Layer /job/unknown is not part of the fixed hierarchy"));
}

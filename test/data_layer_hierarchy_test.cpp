#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_layer_hierarchy.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace phlex::detail;
using phlex::data_cell_index;

TEST_CASE("Data layer hierarchy with ambiguous layer names", "[data model]")
{
  data_layer_hierarchy h;
  CHECK_THROWS(h.count_for("/job"));
  CHECK(h.count_for("/job", true) == 0);

  auto job_index = data_cell_index::job();
  h.increment_count(job_index);
  CHECK(h.count_for("/job") == 1);

  auto spill_index = job_index->make_child("spill", 0);
  h.increment_count(spill_index);

  auto run_index = job_index->make_child("run", 0);
  h.increment_count(run_index);
  CHECK(h.count_for("/job/run") == 1);
  CHECK(h.count_for("run") == 1);

  // Nested spill indices
  h.increment_count(run_index->make_child("spill", 0));
  h.increment_count(run_index->make_child("spill", 1));

  CHECK_THROWS(h.count_for("spill"));
  CHECK(h.count_for("/job/spill") == 1);
  CHECK(h.count_for("/job/run/spill") == 2);
}

TEST_CASE("Data layer hierarchy requires named layers", "[data model]")
{
  data_layer_hierarchy h;
  auto job_index = data_cell_index::job();
  CHECK_NOTHROW(h.increment_count(job_index));
  CHECK_THROWS(job_index->make_child("", 0));
  CHECK_NOTHROW(h.to_string());
}

TEST_CASE("Data layer hierarchy layout", "[data model]")
{
  data_layer_hierarchy h;

  SECTION("Empty hierarchy") { CHECK(h.to_string().empty()); }

  SECTION("Root only")
  {
    h.increment_count(data_cell_index::job());
    CHECK(h.to_string() == "\n\nSeen layers:\n\n  job\n");
  }

  SECTION("Sorted siblings and nested branches")
  {
    auto job = data_cell_index::job();
    auto spill = job->make_child("spill", 0);
    auto run = job->make_child("run", 0);
    h.increment_count(spill->make_child("event", 0));
    h.increment_count(spill);
    h.increment_count(run->make_child("spill", 0));
    h.increment_count(run->make_child("spill", 1));
    h.increment_count(run->make_child("event", 0));
    h.increment_count(run);
    h.increment_count(job);
    CHECK(h.to_string() == R"(

Seen layers:

  job
   │
   ├ run: 1
   │  │
   │  ├ event: 1
   │  │
   │  └ spill: 2
   │
   └ spill: 1
      │
      └ event: 1
)");
  }
}

TEST_CASE("Data layer hierarchy with missing parent", "[data model]")
{
  auto job_index = data_cell_index::job();
  auto run_index = job_index->make_child("run", 0);
  auto event_index = run_index->make_child("event", 0);

  SECTION("Missing job layer")
  {
    data_layer_hierarchy h;
    h.increment_count(run_index);
    h.increment_count(event_index);
    CHECK(h.to_string() == R"(

Seen layers:

  job: (incomplete)
   │
   └ run: 1
      │
      └ event: 1
)");
    CHECK(h.count_for("/job", true) == 0);
  }

  SECTION("Missing intermediate layer")
  {
    data_layer_hierarchy h;
    h.increment_count(job_index);
    h.increment_count(event_index);
    CHECK(h.to_string() == R"(

Seen layers:

  job
   │
   └ run: (incomplete)
      │
      └ event: 1
)");
    CHECK(h.count_for("run", true) == 0);
  }

  SECTION("Complete hierarchy")
  {
    data_layer_hierarchy h;
    h.increment_count(job_index);
    h.increment_count(run_index);
    h.increment_count(event_index);
    CHECK(h.to_string() == R"(

Seen layers:

  job
   │
   └ run: 1
      │
      └ event: 1
)");
  }
}

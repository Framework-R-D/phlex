#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/data_layer_hierarchy.hpp"

#include "catch2/catch_test_macros.hpp"

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

TEST_CASE("Data layer hierarchy with unnamed layer", "[data model]")
{
  // Exercises the maybe_name() fallback to "(unnamed)" for layers with empty names.
  // Invoke print() explicitly so the test body, rather than teardown side effects,
  // traverses the hierarchy and reaches maybe_name("") for the unnamed child.
  data_layer_hierarchy h;
  auto job_index = data_cell_index::job();
  CHECK_NOTHROW(h.increment_count(job_index));
  CHECK_NOTHROW(h.increment_count(job_index->make_child("", 0)));
  CHECK_NOTHROW(h.print());
}

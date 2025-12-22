#include "phlex/model/data_cell_index.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex;

TEST_CASE("Verify independent hashes", "[data model]")
{
  // In the original implementation of the hash algorithm, there was a collision between the hash for
  // "run:0 subrun:0 event: 760" and "run:0 subrun:1 event: 4999".

  auto base = data_cell_index::base_ptr();
  CHECK(base->hash() == 0ull);

  auto run = base->make_child(0, "run");
  auto subrun_0 = run->make_child(0, "subrun");
  auto event_760 = subrun_0->make_child(760, "event");

  auto subrun_1 = run->make_child(1, "subrun");
  CHECK(subrun_0->hash() != subrun_1->hash());

  auto event_4999 = subrun_1->make_child(4999, "event");
  CHECK(event_760->hash() != event_4999->hash());
  CHECK(event_760->layer_hash() == event_4999->layer_hash());
}

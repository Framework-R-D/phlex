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

TEST_CASE("data_cell_index methods", "[data model]")
{
  auto base = data_cell_index::base_ptr();
  auto run0 = base->make_child(0, "run");
  auto run1 = base->make_child(1, "run");

  SECTION("Comparisons")
  {
    CHECK(*run0 < *run1);
    CHECK_FALSE(*run1 < *run0);
    CHECK_FALSE(*run0 < *run0);

    auto subrun0 = run0->make_child(0, "subrun");
    auto subrun1 = run0->make_child(1, "subrun");
    CHECK(*subrun0 < *subrun1);
    CHECK(*run0 < *subrun0);
  }

  SECTION("to_string")
  {
    auto subrun = run0->make_child(5, "subrun");
    CHECK(subrun->to_string() == "[run:0, subrun:5]");
    CHECK(base->to_string() == "[]");

    auto nameless = base->make_child(10, "");
    CHECK(nameless->to_string() == "[10]");
  }

  SECTION("Parent lookup")
  {
    auto subrun = run0->make_child(5, "subrun");
    auto event = subrun->make_child(100, "event");

    CHECK(event->parent("subrun") == subrun);
    CHECK(event->parent("run") == run0);
    CHECK(event->parent("nonexistent") == nullptr);
  }

  SECTION("Layer path")
  {
    auto subrun = run0->make_child(5, "subrun");
    CHECK(subrun->layer_path() == "/job/run/subrun");
  }

  SECTION("Base access") { CHECK(&data_cell_index::base() == data_cell_index::base_ptr().get()); }
}

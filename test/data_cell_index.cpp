#include "phlex/model/data_cell_index.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex;
using namespace phlex::experimental::literals;

TEST_CASE("Verify independent hashes", "[data model]")
{
  // In the original implementation of the hash algorithm, there was a collision between the hash for
  // "run:0 subrun:0 event: 760" and "run:0 subrun:1 event: 4999".

  auto base = data_cell_index::base_ptr();
  CHECK(base->hash() == 0ull);

  auto run = base->make_child(0, "run"_id);
  auto subrun_0 = run->make_child(0, "subrun"_id);
  auto event_760 = subrun_0->make_child(760, "event"_id);

  auto subrun_1 = run->make_child(1, "subrun"_id);
  CHECK(subrun_0->hash() != subrun_1->hash());

  auto event_4999 = subrun_1->make_child(4999, "event"_id);
  CHECK(event_760->hash() != event_4999->hash());
  CHECK(event_760->layer_hash() == event_4999->layer_hash());
}

TEST_CASE("data_cell_index methods", "[data model]")
{
  auto base = data_cell_index::base_ptr();
  auto run0 = base->make_child(0, "run"_id);
  auto run1 = base->make_child(1, "run"_id);

  SECTION("Comparisons")
  {
    CHECK(*run0 < *run1);
    CHECK_FALSE(*run1 < *run0);
    CHECK_FALSE(*run0 < *run0);

    auto subrun0 = run0->make_child(0, "subrun"_id);
    auto subrun1 = run0->make_child(1, "subrun"_id);
    CHECK(*subrun0 < *subrun1);
    CHECK(*run0 < *subrun0);
  }

  SECTION("to_string")
  {
    auto subrun = run0->make_child(5, "subrun"_id);
    CHECK(subrun->to_string() == "[run:0, subrun:5]");
    CHECK(base->to_string() == "[]");

    auto nameless = base->make_child(10, ""_id);
    CHECK(nameless->to_string() == "[10]");
  }

  SECTION("Parent lookup")
  {
    auto subrun = run0->make_child(5, "subrun"_id);
    auto event = subrun->make_child(100, "event"_id);

    CHECK(event->parent("subrun"_id) == subrun);
    CHECK(event->parent("run"_id) == run0);
    CHECK(event->parent("nonexistent"_id) == nullptr);
  }

  SECTION("Layer path")
  {
    auto subrun = run0->make_child(5, "subrun"_id);
    CHECK(subrun->layer_path() == "/job/run/subrun");
  }

  SECTION("Base access") { CHECK(&data_cell_index::base() == data_cell_index::base_ptr().get()); }
}

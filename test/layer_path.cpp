#include "phlex/model/layer_path.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;

TEST_CASE("Layer path tests", "[layer_path]")
{
  layer_path job = "/job";
  layer_path run = "/job/run";
  layer_path lumiblock = "/job/run/lumiblock";
  layer_path subrun = "/job/run/subrun";
  layer_path event = "/job/run/subrun/event";

  identifier event_id = "event";
  layer_path partial_event = "subrun/event";

  CHECK(run.is_complete());
  CHECK(run.is_strict_prefix_of(event));
  CHECK(run.is_strict_prefix_of(subrun));
  CHECK_FALSE(lumiblock.is_strict_prefix_of(event));

  CHECK(event.ends_with(event_id));
  CHECK_FALSE(partial_event.is_complete());
  CHECK(event.ends_with(partial_event));
  CHECK_FALSE(subrun.ends_with(partial_event));

  CHECK(subrun.to_string() == "/job/run/subrun");

  auto event_hashes = event.hashes();
  CHECK(event_hashes.contains(job.hash()));
  CHECK(event_hashes.contains(run.hash()));
  CHECK(event_hashes.contains(subrun.hash()));
  CHECK(event_hashes.contains(event.hash()));
  CHECK_FALSE(event_hashes.contains(lumiblock.hash()));

  // Validation
  CHECK_THROWS(layer_path(""));
  CHECK_THROWS(layer_path("/notajob/notarun"));
  CHECK_THROWS(layer_path(std::vector<identifier>{}));
  CHECK_THROWS(layer_path("/job/run/job"));
  CHECK_THROWS(layer_path("subrun/job"));
}

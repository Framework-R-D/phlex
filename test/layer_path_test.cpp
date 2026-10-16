#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace phlex::experimental;

TEST_CASE("Layer path tests", "[layer_path]")
{
  layer_path const job = "/job";
  layer_path const run = "/job/run";
  layer_path const lumiblock = "/job/run/lumiblock";
  layer_path const subrun = "/job/run/subrun";
  layer_path const event = "/job/run/subrun/event";

  identifier const event_id = "event";
  layer_path const partial_event = "subrun/event";

  CHECK(run.is_complete());
  CHECK(run.is_strict_prefix_of(event));
  CHECK(run.is_strict_prefix_of(subrun));
  CHECK_FALSE(lumiblock.is_strict_prefix_of(event));

  CHECK(event.ends_with(event_id));
  CHECK_FALSE(partial_event.is_complete());
  CHECK(event.ends_with(partial_event));
  CHECK_FALSE(subrun.ends_with(partial_event));

  CHECK(event.contains("job"));
  CHECK(event.contains("run"));
  CHECK(event.contains(event_id));
  CHECK_FALSE(event.contains("lumiblock"));
  CHECK(partial_event.contains("subrun"));
  CHECK(partial_event.contains(event_id));
  CHECK_FALSE(partial_event.contains("job"));

  auto const components = event.components();
  REQUIRE(components.size() == 4);
  CHECK(components[0] == "job");
  CHECK(components[1] == "run");
  CHECK(components[2] == "subrun");
  CHECK(components[3] == "event");

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

#include "phlex/core/index_router.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <utility>

using namespace phlex;

TEST_CASE("Duplicate counting paths deliver one combined end token", "[graph][issue955]")
{
  using namespace phlex::experimental::literals;
  auto const [description, top_count, nested_count] =
    GENERATE(Catch::Generators::table<char const*, unsigned int, unsigned int>(
      {{"Both event paths populated", 2u, 3u},
       {"Top-level event path empty", 0u, 3u},
       {"Nested event path empty", 2u, 0u},
       {"Both event paths empty", 0u, 0u}}));
  CAPTURE(description, top_count, nested_count);

  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/event"}, experimental::layer_path{"/job/run/event"}},
    {},
    {{"dummy", {.input_product = {.layer = "job"}, .port = &indices}}},
    {},
    {{"fold",
      {{.layer = "job"_id,
        .counting_layer = "event"_id,
        .token_port = &tokens,
        .index_port = &indices}}}});

  auto job = data_cell_index::job();
  auto run = job->make_child("run", 0);
  router.route(job, {});
  router.route(run, {});
  auto job_counts = std::make_shared<detail::data_cell_counts>();
  job_counts->emplace(job->make_child("event", 0)->layer_hash(), top_count);
  job_counts->emplace(run->layer_hash(), 1);
  router.drain({{.index = job, .counts = job_counts}});
  graph.wait_for_all();
  detail::indexed_end_token token;
  CHECK_FALSE(tokens.try_get(token));

  auto run_counts = std::make_shared<detail::data_cell_counts>();
  run_counts->emplace(run->make_child("event", 0)->layer_hash(), nested_count);
  router.drain({{.index = run, .counts = run_counts}});
  graph.wait_for_all();
  REQUIRE(tokens.try_get(token));
  CHECK(token.index == job);
  CHECK(std::cmp_equal(token.count, top_count + nested_count));
  CHECK_FALSE(tokens.try_get(token));
}

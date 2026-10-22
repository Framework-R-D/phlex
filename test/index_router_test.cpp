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
#include <catch2/matchers/catch_matchers.hpp>
#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <string>
#include <utility>

using namespace phlex;

TEST_CASE("Index routing requires an explicit driver hierarchy", "[graph]")
{
  tbb::flow::graph graph;
  detail::index_router router{graph};
  CHECK_THROWS_WITH(router.finalize(graph, {}, {}, {}, {}, {}),
                    "Cannot finalize index_router with an empty driver hierarchy.");
  CHECK_NOTHROW(router.finalize(graph, {experimental::layer_path{"/job"}}, {}, {}, {}, {}));
  CHECK_NOTHROW(router.route(data_cell_index::job(), {}));
  CHECK_THROWS_WITH(router.route(data_cell_index::job()->make_child("run", 0), {}),
                    "Index layer path '/job/run' is not in the established layer hierarchy.");
  graph.wait_for_all();
}

TEST_CASE("Index routing rejects undeclared layer paths", "[graph]")
{
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/run/event"}, experimental::layer_path{"/job/spill"}},
    {},
    {{"dummy", {.input_product = {.layer = "event"}, .port = &indices}}},
    {},
    {});

  auto job = data_cell_index::job();
  auto run = job->make_child("run", 0);
  CHECK_NOTHROW(router.route(run->make_child("event", 0), {}));
  auto unknown = job->make_child("spill", 0)->make_child("event", 0);
  CHECK_THROWS_WITH(
    router.route(unknown, {}),
    "Index layer path '/job/spill/event' is not in the established layer hierarchy.");
  graph.wait_for_all();
}

TEST_CASE("Index routing roots relative driver paths at the job layer", "[graph]")
{
  tbb::flow::graph graph;
  detail::index_router router{graph};
  router.finalize(graph, {experimental::layer_path{"run/event"}}, {}, {}, {}, {});

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  auto const event = run->make_child("event", 0);
  CHECK_NOTHROW(router.route(job, {}));
  CHECK_NOTHROW(router.route(run, {}));
  CHECK_NOTHROW(router.route(event, {}));
  CHECK_THROWS_WITH(router.route(job->make_child("event", 0), {}),
                    "Index layer path '/job/event' is not in the established layer hierarchy.");
  graph.wait_for_all();
}

TEST_CASE("Unfold hierarchy resolves dependencies before their producers", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/spill"}},
    {{.name = "a_split_b", .input_layers = {"a"_id, "spill"_id}, .output_layer = "b"_id},
     {.name = "z_split_a", .input_layers = {"spill"_id}, .output_layer = "a"_id}},
    {{"dummy", {.input_product = {.layer = "spill"}, .port = &indices}}},
    {},
    {});

  auto spill = data_cell_index::job()->make_child("spill", 0);
  auto a = spill->make_child("a", 0);
  auto b = a->make_child("b", 0);
  CHECK_NOTHROW(router.route(spill, {}));
  CHECK_NOTHROW(router.route(a, {}));
  CHECK_NOTHROW(router.route(b, {}));
  graph.wait_for_all();
}

TEST_CASE("Unfold hierarchy rejects cyclic layer production", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  detail::index_router router{graph};
  detail::index_router::unfold_data unfolds;
  std::string expected_error;

  SECTION("Self-cycle")
  {
    unfolds = {{.name = "split_run", .input_layers = {"run"_id}, .output_layer = "run"_id}};
    expected_error =
      "Unfold layer hierarchy expansion exceeded max depth 3 (initial deepest 2 + 1 unfold "
      "node(s)). Offending unfold: split_run";
  }

  SECTION("Two-unfold cycle")
  {
    unfolds = {{.name = "split_spill", .input_layers = {"run"_id}, .output_layer = "spill"_id},
               {.name = "split_run", .input_layers = {"spill"_id}, .output_layer = "run"_id}};
    expected_error =
      "Unfold layer hierarchy expansion exceeded max depth 4 (initial deepest 2 + 2 unfold "
      "node(s)). Offending unfold: split_spill";
  }

  CHECK_THROWS_WITH(
    router.finalize(graph,
                    {experimental::layer_path{"/job/run"}},
                    unfolds,
                    {{"dummy", {.input_product = {.layer = "run"}, .port = &indices}}},
                    {},
                    {}),
    expected_error);
}

TEST_CASE("Unfold hierarchy rejects driver output layer collisions", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  using experimental::layer_path;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  detail::index_router router{graph};
  auto const driver_path =
    GENERATE(layer_path{"/job/run/spill"}, layer_path{"/job/run/spill/event"});
  CAPTURE(driver_path);

  CHECK_THROWS_WITH(
    router.finalize(graph,
                    {driver_path},
                    {{.name = "split", .input_layers = {"run"_id}, .output_layer = "spill"_id}},
                    {{"dummy", {.input_product = {.layer = "run"}, .port = &indices}}},
                    {},
                    {}),
    "Unfold 'split' specifies output layer '/job/run/spill' already declared by the driver.");
}

TEST_CASE("Unfold hierarchy rejects duplicate output layers", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  using experimental::layer_path;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  detail::index_router router{graph};

  auto const driver_path = GENERATE(layer_path{"/job/run"}, layer_path{"/job"});
  CAPTURE(driver_path);
  detail::index_router::unfold_data unfolds{
    {.name = "split_a", .input_layers = {"run"_id}, .output_layer = "spill"_id},
    {.name = "split_b", .input_layers = {"run"_id}, .output_layer = "spill"_id}};
  if (driver_path == layer_path{"/job"}) {
    unfolds.push_back({.name = "split_run", .input_layers = {"job"_id}, .output_layer = "run"_id});
  }

  CHECK_THROWS_WITH(
    router.finalize(graph,
                    {driver_path},
                    unfolds,
                    {{"dummy", {.input_product = {.layer = "run"}, .port = &indices}}},
                    {},
                    {}),
    "Unfolds 'split_a' and 'split_b' specify the same output layer '/job/run/spill'.");
}

TEST_CASE("Counting layers on other branches do not produce end tokens", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/run/calib"}, experimental::layer_path{"/job/spill/event"}},
    {},
    {},
    {},
    {{"fold",
      {{.layer = "run"_id,
        .counting_layer = "event"_id,
        .token_port = &tokens,
        .index_port = &indices}}}});

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  router.route(job, {});
  router.route(run, {});
  graph.wait_for_all();
  detail::index_message message;
  REQUIRE(indices.try_get(message));
  CHECK(message.index == run);
  CHECK_FALSE(indices.try_get(message));
  detail::indexed_end_token token;
  CHECK_FALSE(tokens.try_get(token));

  auto run_counts = std::make_shared<detail::data_cell_counts>();
  run_counts->emplace(run->make_child("calib", 0)->layer_hash(), 0);
  auto job_counts = std::make_shared<detail::data_cell_counts>();
  job_counts->emplace(run->layer_hash(), 1);
  job_counts->emplace(job->make_child("spill", 0)->layer_hash(), 0);
  router.drain({{.index = run, .counts = run_counts}, {.index = job, .counts = job_counts}});
  graph.wait_for_all();
  CHECK_FALSE(tokens.try_get(token));
}

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

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
#include <fmt/format.h>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace phlex;

TEST_CASE("Index routing requires an explicit driver hierarchy", "[graph]")
{
  tbb::flow::graph graph;
  detail::index_router router{graph};
  CHECK_THROWS_WITH(router.finalize(graph, {}, {}, {}, {}),
                    "Cannot finalize index_router with an empty driver hierarchy.");
  CHECK_NOTHROW(router.finalize(graph, {experimental::layer_path{"/job"}}, {}, {}, {}));
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
    {});

  auto job = data_cell_index::job();
  auto run = job->make_child("run", 0);
  CHECK_NOTHROW(router.route(run->make_child("event", 0), {}));
  auto unknown = job->make_child("spill", 0)->make_child("event", 0);
  // A known terminal name does not make a different parent path eligible.
  CHECK_THROWS_WITH(
    router.route(unknown, {}),
    "Index layer path '/job/spill/event' is not in the established layer hierarchy.");
  graph.wait_for_all();
}

TEST_CASE("Index routing roots relative driver paths at the job layer", "[graph]")
{
  tbb::flow::graph graph;
  detail::index_router router{graph};
  router.finalize(graph, {experimental::layer_path{"run/event"}}, {}, {}, {});

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
  tbb::flow::queue_node<detail::indexed_end_token> data_tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/event"}, experimental::layer_path{"/job/run/event"}},
    {},
    {{"dummy", {.input_product = {.layer = "job"}, .port = &indices}}},
    {{"fold",
      {.input_layers = {"event"_id},
       .participants = {
         {.layer = "job"_id, .index_port = &indices, .completion_port = &tokens},
         {.layer = "event"_id, .index_port = &indices, .completion_port = &data_tokens}}}}});

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
  // Completion must wait for the nested counting path, even when its count will be zero.
  CHECK_FALSE(tokens.try_get(token));

  auto run_counts = std::make_shared<detail::data_cell_counts>();
  run_counts->emplace(run->make_child("event", 0)->layer_hash(), nested_count);
  router.drain({{.index = run, .counts = run_counts}});
  graph.wait_for_all();
  REQUIRE(tokens.try_get(token));
  CHECK(token.index == job);
  CHECK(std::cmp_equal(token.count, top_count + nested_count));
  // Combine both counting paths into one token; the pass-through participant gets none.
  CHECK_FALSE(tokens.try_get(token));
  CHECK_FALSE(data_tokens.try_get(token));
}

TEST_CASE("Join participants share partition-constrained invocation accounting", "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  auto const count = GENERATE(0u, 3u);
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> provider_indices{graph};
  tbb::flow::queue_node<detail::index_message> accumulator_indices{graph};
  tbb::flow::queue_node<detail::index_message> parent_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> accumulator_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> parent_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/run/event/subevent"},
     experimental::layer_path{"/job/event/subevent"}},
    {},
    {{"dummy", {.input_product = {.layer = "job"}, .port = &provider_indices}}},
    {{"fold",
      {.input_layers = {"event"_id, "subevent"_id},
       .participants = {
         {.layer = "run"_id,
          .index_port = &accumulator_indices,
          .completion_port = &accumulator_tokens},
         {.layer = "event"_id, .index_port = &parent_indices, .completion_port = &parent_tokens},
         {.layer = "subevent"_id,
          .index_port = &child_indices,
          .completion_port = &child_tokens}}}}});

  auto job = data_cell_index::job();
  auto run = job->make_child("run", 0);
  auto event = run->make_child("event", 0);
  auto excluded = job->make_child("event", 0);
  router.route(job, {});
  router.route(run, {});
  router.route(event, {});
  router.route(excluded, {});
  router.route(excluded->make_child("subevent", 0), {});
  for (unsigned int i = 0; i != count; ++i) {
    router.route(event->make_child("subevent", i), {});
  }
  auto event_counts = std::make_shared<detail::data_cell_counts>();
  event_counts->emplace(event->make_child("subevent", 0)->layer_hash(), count);
  router.drain({{.index = event, .counts = event_counts}});
  auto run_counts = std::make_shared<detail::data_cell_counts>();
  run_counts->emplace(event->layer_hash(), 1);
  router.drain({{.index = run, .counts = run_counts}});
  auto excluded_counts = std::make_shared<detail::data_cell_counts>();
  excluded_counts->emplace(excluded->make_child("subevent", 0)->layer_hash(), 1);
  router.drain({{.index = excluded, .counts = excluded_counts}});
  graph.wait_for_all();

  detail::indexed_end_token token;
  // Cached partitions get exactly one completion each; pass-through children get none.
  REQUIRE(accumulator_tokens.try_get(token));
  CHECK(token.index == run);
  CHECK(std::cmp_equal(token.count, count));
  CHECK_FALSE(accumulator_tokens.try_get(token));
  REQUIRE(parent_tokens.try_get(token));
  CHECK(token.index == event);
  CHECK(std::cmp_equal(token.count, count));
  CHECK_FALSE(parent_tokens.try_get(token));
  CHECK_FALSE(child_tokens.try_get(token));

  detail::index_message index;
  for (unsigned int i = 0; i != count; ++i) {
    REQUIRE(accumulator_indices.try_get(index));
    CHECK(index.index == run);
    CHECK(index.cache);
    auto const message_id = index.msg_id;
    REQUIRE(parent_indices.try_get(index));
    CHECK(index.index == event);
    CHECK(index.cache);
    CHECK(index.msg_id == message_id);
    REQUIRE(child_indices.try_get(index));
    CHECK(index.index->parent() == event);
    CHECK_FALSE(index.cache);
    CHECK(index.msg_id == message_id);
  }
  // The excluded branch must not add invocations, including when the eligible count is zero.
  CHECK_FALSE(accumulator_indices.try_get(index));
  CHECK_FALSE(parent_indices.try_get(index));
  CHECK_FALSE(child_indices.try_get(index));
}

TEST_CASE("Fold rejects partitions without strict descendant invocations", "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  // The partition is on a sibling branch, absent, or equal to the invocation layer.
  phlex::experimental::identifier const partition = GENERATE("run", "spill", "event");
  CAPTURE(partition);
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::index_message> partition_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  CHECK_THROWS_WITH(
    router.finalize(graph,
                    {experimental::layer_path{"/job/event"}, experimental::layer_path{"/job/run"}},
                    {},
                    {},
                    {{"fold",
                      {.input_layers = {"event"_id},
                       .participants = {{.layer = partition,
                                         .index_port = &indices,
                                         .completion_port = &tokens,
                                         .partition_port = &partition_indices},
                                        {.layer = "event"_id,
                                         .index_port = &indices,
                                         .completion_port = &tokens}}}}}),
    fmt::format("Fold 'fold' partition layer '{}' is not a strict ancestor of any eligible "
                "invocation path for input layers [event].",
                partition));
}

TEST_CASE("Empty folds accept a partition shared with an ancestor input", "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::index_message> partition_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/run/event"}},
    {},
    {},
    {{"fold",
      {.input_layers = {"run"_id, "event"_id},
       .participants = {
         {.layer = "run"_id,
          .index_port = &indices,
          .completion_port = &tokens,
          .partition_port = &partition_indices},
         {.layer = "run"_id, .index_port = &indices, .completion_port = &tokens},
         {.layer = "event"_id, .index_port = &indices, .completion_port = &tokens}}}}});

  auto const run = data_cell_index::job()->make_child("run", 0);
  router.route(run, {});
  auto counts = std::make_shared<detail::data_cell_counts>();
  counts->emplace(run->make_child("event", 0)->layer_hash(), 0);
  router.drain({{.index = run, .counts = counts}});
  graph.wait_for_all();

  detail::index_message index;
  REQUIRE(partition_indices.try_get(index));
  CHECK(index.index == run);
  CHECK_FALSE(partition_indices.try_get(index));
  CHECK_FALSE(indices.try_get(index));

  detail::indexed_end_token token;
  // Both the accumulator and the ancestor input complete with zero invocations.
  for (unsigned int i = 0; i != 2; ++i) {
    REQUIRE(tokens.try_get(token));
    CHECK(token.index == run);
    CHECK(token.count == 0);
  }
  CHECK_FALSE(tokens.try_get(token));
}

TEST_CASE("Cells outside a participant's storage paths get no completion or partition message",
          "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> provider_indices{graph};
  tbb::flow::queue_node<detail::index_message> cached_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::index_message> partition_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> cached_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/run/event"}, experimental::layer_path{"/job/other/run"}},
    {},
    {{"dummy", {.input_product = {.layer = "job"}, .port = &provider_indices}}},
    {{"join",
      {.input_layers = {"event"_id},
       .participants = {
         {.layer = "run"_id,
          .index_port = &cached_indices,
          .completion_port = &cached_tokens,
          .partition_port = &partition_indices},
         {.layer = "event"_id, .index_port = &child_indices, .completion_port = &child_tokens}}}}});

  auto job = data_cell_index::job();
  auto run = job->make_child("run", 0);
  auto event = run->make_child("event", 0);
  // The run under "other" shares the participant's name but is not an ancestor of any invocation.
  auto excluded = job->make_child("other", 0)->make_child("run", 0);
  router.route(job, {});
  router.route(run, {});
  router.route(event, {});
  router.route(excluded, {});
  auto run_counts = std::make_shared<detail::data_cell_counts>();
  run_counts->emplace(event->layer_hash(), 1);
  router.drain({{.index = run, .counts = run_counts}});
  graph.wait_for_all();

  detail::index_message index;
  REQUIRE(partition_indices.try_get(index));
  CHECK(index.index == run);
  CHECK_FALSE(partition_indices.try_get(index));

  detail::indexed_end_token token;
  REQUIRE(cached_tokens.try_get(token));
  CHECK(token.index == run);
  CHECK(token.count == 1);
  CHECK_FALSE(cached_tokens.try_get(token));
  CHECK_FALSE(child_tokens.try_get(token));

  REQUIRE(cached_indices.try_get(index));
  CHECK(index.index == run);
  CHECK_FALSE(cached_indices.try_get(index));
  REQUIRE(child_indices.try_get(index));
  CHECK(index.index == event);
  CHECK_FALSE(child_indices.try_get(index));
}

TEST_CASE(
  "Cached participants learn their accepted cells only when same-named cells exist elsewhere",
  "[graph]")
{
  using namespace phlex::experimental::literals;
  auto const with_foreign = GENERATE(false, true);
  CAPTURE(with_foreign);
  std::vector<experimental::layer_path> paths{experimental::layer_path{"/job/run/event"}};
  if (with_foreign) {
    paths.emplace_back("/job/other/run");
  }

  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> provider_indices{graph};
  tbb::flow::queue_node<detail::index_message> cached_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> cached_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};
  std::optional<std::unordered_set<std::size_t>> cached_accepted;
  bool pass_through_notified = false;
  detail::index_router router{graph};
  router.finalize(graph,
                  paths,
                  {},
                  {{"dummy", {.input_product = {.layer = "job"}, .port = &provider_indices}}},
                  {{"join",
                    {.input_layers = {"event"_id},
                     .participants = {{.layer = "run"_id,
                                       .index_port = &cached_indices,
                                       .completion_port = &cached_tokens,
                                       .accept_cells =
                                         [&cached_accepted](std::unordered_set<std::size_t> cells) {
                                           cached_accepted = std::move(cells);
                                         }},
                                      {.layer = "event"_id,
                                       .index_port = &child_indices,
                                       .completion_port = &child_tokens,
                                       .accept_cells = [&pass_through_notified](
                                                         std::unordered_set<std::size_t> const&) {
                                         pass_through_notified = true;
                                       }}}}}});

  CHECK(cached_accepted.has_value() == with_foreign);
  if (with_foreign) {
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- checked above
    CHECK(*cached_accepted ==
          std::unordered_set<std::size_t>{experimental::layer_path{"/job/run"}.hash()});
  }
  // The foreign run path has no event layer, so this input needs no filtering.
  CHECK_FALSE(pass_through_notified);
}

TEST_CASE("Pass-through participants learn eligible paths when same-named cells exist elsewhere",
          "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  auto const with_foreign = GENERATE(false, true);
  CAPTURE(with_foreign);
  std::vector<experimental::layer_path> paths{experimental::layer_path{"/job/run/event"}};
  if (with_foreign) {
    paths.emplace_back("/job/event");
  }

  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> provider_indices{graph};
  tbb::flow::queue_node<detail::index_message> cached_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> cached_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};
  std::optional<std::unordered_set<std::size_t>> accepted;
  detail::index_router router{graph};
  router.finalize(
    graph,
    paths,
    {},
    {{"dummy", {.input_product = {.layer = "event"}, .port = &provider_indices}}},
    {{"fold",
      {.input_layers = {"event"_id},
       .participants = {
         {.layer = "run"_id, .index_port = &cached_indices, .completion_port = &cached_tokens},
         {.layer = "event"_id,
          .index_port = &child_indices,
          .completion_port = &child_tokens,
          .accept_cells = [&accepted](std::unordered_set<std::size_t> cells) {
            accepted = std::move(cells);
          }}}}}});

  if (with_foreign) {
    REQUIRE(accepted.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- checked above
    CHECK(*accepted ==
          std::unordered_set<std::size_t>{experimental::layer_path{"/job/run/event"}.hash()});
  } else {
    CHECK_FALSE(accepted.has_value());
  }
}

TEST_CASE("Join rejects participants with mixed caching roles", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  CHECK_THROWS_AS(
    router.finalize(
      graph,
      {experimental::layer_path{"/job/a/b"}, experimental::layer_path{"/job/b/a"}},
      {},
      {{"dummy", {.input_product = {.layer = "job"}, .port = &indices}}},
      {{"join",
        {.input_layers = {"a"_id, "b"_id},
         .participants =
           {{.layer = "a"_id, .index_port = &indices, .completion_port = &tokens},
            {.layer = "b"_id, .index_port = &indices, .completion_port = &tokens}}}}}),
    std::runtime_error);
}

TEST_CASE("Join completion follows nearest repeated ancestor", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> provider_indices{graph};
  tbb::flow::queue_node<detail::index_message> parent_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> parent_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/a/a/b"}},
    {},
    {{"dummy", {.input_product = {.layer = "job"}, .port = &provider_indices}}},
    {{"join",
      {.input_layers = {"a"_id, "b"_id},
       .participants = {
         {.layer = "a"_id, .index_port = &parent_indices, .completion_port = &parent_tokens},
         {.layer = "b"_id, .index_port = &child_indices, .completion_port = &child_tokens}}}}});
  auto job = data_cell_index::job();
  auto outer = job->make_child("a", 0);
  auto inner = outer->make_child("a", 0);
  auto child = inner->make_child("b", 0);
  router.route(job, {});
  router.route(outer, {});
  router.route(inner, {});
  router.route(child, {});
  auto inner_counts = std::make_shared<detail::data_cell_counts>();
  inner_counts->emplace(child->layer_hash(), 1);
  router.drain({{.index = inner, .counts = inner_counts}});
  graph.wait_for_all();
  detail::indexed_end_token token;
  REQUIRE(parent_tokens.try_get(token));
  CHECK(token.index == inner);
  CHECK(token.count == 1);
  auto outer_counts = std::make_shared<detail::data_cell_counts>();
  outer_counts->emplace(inner->layer_hash(), 1);
  router.drain({{.index = outer, .counts = outer_counts}});
  graph.wait_for_all();
  // The outer "a" is shadowed by the inner one and owns no invocations.
  CHECK_FALSE(parent_tokens.try_get(token));
  // The child is pass-through and needs no completion token.
  CHECK_FALSE(child_tokens.try_get(token));
  detail::index_message index;
  REQUIRE(parent_indices.try_get(index));
  CHECK(index.index == inner);
  CHECK(index.cache);
  // One invocation projects only to the nearest "a", not to both occurrences.
  CHECK_FALSE(parent_indices.try_get(index));
}

TEST_CASE("Unfold hierarchy resolves dependencies before their producers", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> data_tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/spill"}},
    // The dependent comes first both in this list and in lexical node-name order.
    {{.name = "a_split_b", .input_layers = {"a"_id, "spill"_id}, .output_layer = "b"_id},
     {.name = "z_split_a", .input_layers = {"spill"_id}, .output_layer = "a"_id}},
    {{"dummy", {.input_product = {.layer = "spill"}, .port = &indices}}},
    {{"fold",
      {.input_layers = {"b"_id},
       .participants = {
         {.layer = "spill"_id, .index_port = &indices, .completion_port = &tokens},
         {.layer = "b"_id, .index_port = &indices, .completion_port = &data_tokens}}}}});

  auto spill = data_cell_index::job()->make_child("spill", 0);
  auto a = spill->make_child("a", 0);
  auto b = a->make_child("b", 0);
  router.route(spill, {});
  router.route(a, {});
  auto spill_counts = std::make_shared<detail::data_cell_counts>();
  spill_counts->emplace(a->layer_hash(), 1);
  router.drain({{.index = spill, .counts = spill_counts}});
  graph.wait_for_all();
  detail::indexed_end_token token;
  // The spill cannot complete until the nested unfold supplies its child count.
  CHECK_FALSE(tokens.try_get(token));

  router.unfold_flush_receiver().try_put({.index = a, .layer_hash = b->layer_hash(), .count = 3});
  graph.wait_for_all();
  REQUIRE(tokens.try_get(token));
  CHECK(token.index == spill);
  CHECK(token.count == 3);
  // Emit one spill completion and none for the pass-through b participant.
  CHECK_FALSE(tokens.try_get(token));
  CHECK_FALSE(data_tokens.try_get(token));
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
    // When the driver path is '/job', an unfold is needed to create the 'run' layer as
    // 'split_a' and 'split_b' both depend on it.
    unfolds.push_back({.name = "split_run", .input_layers = {"job"_id}, .output_layer = "run"_id});
  }

  CHECK_THROWS_WITH(
    router.finalize(graph,
                    {driver_path},
                    unfolds,
                    {{"dummy", {.input_product = {.layer = "run"}, .port = &indices}}},
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
    {{"fold",
      {.input_layers = {"event"_id},
       .participants = {
         {.layer = "run"_id, .index_port = &indices, .completion_port = &tokens},
         {.layer = "event"_id, .index_port = &indices, .completion_port = &tokens}}}}});

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  router.route(job, {});
  router.route(run, {});
  graph.wait_for_all();
  detail::index_message message;
  // No invocation path contains both participants, so the join receives no indices.
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

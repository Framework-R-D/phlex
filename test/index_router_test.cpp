#include "phlex/core/index_router.hpp"
#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <fmt/format.h>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace phlex;

namespace {
  struct join_fixture {
    explicit join_fixture(tbb::flow::graph& graph) :
      placeholder_indices_{graph}, placeholder_tokens_{graph}
    {
    }

    std::string name{"placeholder_fold_join"};
    std::vector<experimental::identifier> input_layers;

    detail::join_participant& add_participant(experimental::identifier layer)
    {
      return add_participant(std::move(layer), placeholder_indices_, placeholder_tokens_);
    }

    detail::join_participant& add_participant(
      experimental::identifier layer,
      tbb::flow::receiver<detail::index_message>& indices,
      tbb::flow::receiver<detail::indexed_end_token>& completions)
    {
      participants_.push_back(
        {.layer = std::move(layer), .index_port = &indices, .completion_port = &completions});
      return participants_.back();
    }

    bool empty() const { return participants_.empty(); }

    detail::join_registration registration() const
    {
      return {.input_layers = input_layers, .participants = participants_};
    }

  private:
    std::vector<detail::join_participant> participants_;
    tbb::flow::queue_node<detail::index_message> placeholder_indices_;
    tbb::flow::queue_node<detail::indexed_end_token> placeholder_tokens_;
  };

  struct router_fixture {
    explicit router_fixture(tbb::flow::graph& graph) : join{graph}, graph_{graph}, router_{graph} {}

    std::vector<experimental::layer_path> driver_paths;
    detail::index_router::unfold_data unfolds;
    detail::index_router::provider_input_port_t provider;
    detail::index_router::provider_input_ports_t providers;
    join_fixture join;

    void finalize()
    {
      auto provider_ports = providers;
      if (provider.port != nullptr) {
        provider_ports.emplace("placeholder_provider", provider);
      }
      std::map<std::string, detail::join_registration> joins;
      if (!join.empty()) {
        joins.emplace(join.name, join.registration());
      }
      router_.finalize(graph_, driver_paths, unfolds, std::move(provider_ports), joins);
    }

    data_cell_index_ptr route(data_cell_index_ptr const& index,
                              detail::index_flushes const& flushes)
    {
      return router_.route(index, flushes);
    }

    void drain(detail::index_flushes const& flushes) { router_.drain(flushes); }

    auto& unfold_flush_receiver() { return router_.unfold_flush_receiver(); }

  private:
    tbb::flow::graph& graph_;
    detail::index_router router_;
  };
}

TEST_CASE("Unresolved provider layers do not receive concrete indices", "[graph]")
{
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> first_unresolved{graph};
  tbb::flow::queue_node<detail::index_message> second_unresolved{graph};
  tbb::flow::queue_node<detail::index_message> resolved{graph};
  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run"}};
  router.providers = {{"first_unresolved", {.input_product = {}, .port = &first_unresolved}},
                      {"second_unresolved", {.input_product = {}, .port = &second_unresolved}},
                      {"resolved", {.input_product = {.layer = "run"}, .port = &resolved}}};
  router.finalize();

  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 0);
  router.route(job, {});
  router.route(run, {});
  graph.wait_for_all();

  detail::index_message message;
  REQUIRE(resolved.try_get(message));
  CHECK(message.index == run);
  CHECK_FALSE(resolved.try_get(message));
  CHECK_FALSE(first_unresolved.try_get(message));
  CHECK_FALSE(second_unresolved.try_get(message));
}

TEST_CASE("Index routing requires an explicit driver hierarchy", "[graph]")
{
  tbb::flow::graph graph;
  router_fixture router{graph};
  CHECK_THROWS_WITH(router.finalize(),
                    "Cannot finalize index_router with an empty driver hierarchy.");
  router.driver_paths = {experimental::layer_path{"/job"}};
  CHECK_NOTHROW(router.finalize());
  CHECK_NOTHROW(router.route(data_cell_index::job(), {}));
  CHECK_THROWS_WITH(router.route(data_cell_index::job()->make_child("run", 0), {}),
                    "Index layer path '/job/run' is not in the established layer hierarchy.");
  graph.wait_for_all();
}

TEST_CASE("Index routing rejects undeclared layer paths", "[graph]")
{
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/event"},
                         experimental::layer_path{"/job/spill"}};
  router.provider = {.input_product = {.layer = "event"}, .port = &placeholder_indices};
  router.finalize();

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
  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"run/event"}};
  router.finalize();

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

TEST_CASE("Index routing accepts data cells from chained unfolds", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> data_tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/spill"}};
  // The dependent comes first both in this list and in lexical node-name order.
  router.unfolds = {
    {.name = "a_split_b", .input_layers = {"a"_id, "spill"_id}, .output_layer = "b"_id},
    {.name = "z_split_a", .input_layers = {"spill"_id}, .output_layer = "a"_id}};
  router.provider = {.input_product = {.layer = "spill"}, .port = &placeholder_indices};
  router.join.input_layers = {"b"_id};
  router.join.add_participant("spill"_id, placeholder_indices, tokens);
  router.join.add_participant("b"_id, placeholder_indices, data_tokens);
  router.finalize();

  auto spill = data_cell_index::job()->make_child("spill", 0);
  auto a = spill->make_child("a", 0);
  auto b = a->make_child("b", 0);
  CHECK_NOTHROW(router.route(spill, {}));
  CHECK_NOTHROW(router.route(a, {}));
  CHECK_NOTHROW(router.route(b, {}));
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

TEST_CASE("Join participants share partition-constrained invocation accounting", "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  auto const count = GENERATE(0u, 3u);
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  tbb::flow::queue_node<detail::index_message> accumulator_indices{graph};
  tbb::flow::queue_node<detail::index_message> parent_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> accumulator_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> parent_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/event/subevent"},
                         experimental::layer_path{"/job/event/subevent"}};
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.input_layers = {"event"_id, "subevent"_id};
  router.join.add_participant("run"_id, accumulator_indices, accumulator_tokens);
  router.join.add_participant("event"_id, parent_indices, parent_tokens);
  router.join.add_participant("subevent"_id, child_indices, child_tokens);
  router.finalize();

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
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/event"},
                         experimental::layer_path{"/job/run"}};
  router.join.name = "fold_join";
  router.join.input_layers = {"event"_id};
  router.join.add_participant(partition).partition_port = &placeholder_indices;
  router.join.add_participant("event"_id);
  CHECK_THROWS_WITH(
    router.finalize(),
    fmt::format("Fold 'fold_join' partition layer '{}' is not a strict ancestor of any eligible "
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

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/event"}};
  router.join.input_layers = {"run"_id, "event"_id};
  router.join.add_participant("run"_id, indices, tokens).partition_port = &partition_indices;
  router.join.add_participant("run"_id, indices, tokens);
  router.join.add_participant("event"_id, indices, tokens);
  router.finalize();

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
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  tbb::flow::queue_node<detail::index_message> cached_indices{graph};
  tbb::flow::queue_node<detail::index_message> child_indices{graph};
  tbb::flow::queue_node<detail::index_message> partition_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> cached_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/event"},
                         experimental::layer_path{"/job/other/run"}};
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.input_layers = {"event"_id};
  router.join.add_participant("run"_id, cached_indices, cached_tokens).partition_port =
    &partition_indices;
  router.join.add_participant("event"_id, child_indices, child_tokens);
  router.finalize();

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

TEST_CASE("Join participants filter cells only when same-named paths exist elsewhere",
          "[graph][fold]")
{
  using namespace phlex::experimental::literals;
  auto const [foreign_path, filter_cached, filter_child] = GENERATE(
    Catch::Generators::table<char const*, bool, bool>({{"/placeholder/for/test", false, false},
                                                       {"/job/other/run", true, false},
                                                       {"/job/event", false, true}}));
  CAPTURE(foreign_path);
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  std::optional<std::unordered_set<std::size_t>> cached_accepted;
  std::optional<std::unordered_set<std::size_t>> child_accepted;

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/event"}};
  if (filter_cached or filter_child) {
    router.driver_paths.emplace_back(foreign_path);
  }
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.input_layers = {"event"_id};
  router.join.add_participant("run"_id).accept_cells =
    [&cached_accepted](std::unordered_set<std::size_t> cells) {
      cached_accepted = std::move(cells);
    };
  router.join.add_participant("event"_id).accept_cells =
    [&child_accepted](std::unordered_set<std::size_t> cells) { child_accepted = std::move(cells); };
  router.finalize();

  std::optional<std::unordered_set<std::size_t>> expected_cached;
  std::optional<std::unordered_set<std::size_t>> expected_child;
  if (filter_cached) {
    expected_cached = std::unordered_set<std::size_t>{experimental::layer_path{"/job/run"}.hash()};
  }
  if (filter_child) {
    expected_child =
      std::unordered_set<std::size_t>{experimental::layer_path{"/job/run/event"}.hash()};
  }
  CHECK(cached_accepted == expected_cached);
  CHECK(child_accepted == expected_child);
}

TEST_CASE("Join rejects an ambiguous configuration", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/a/b"},
                         experimental::layer_path{"/job/b/a"}};
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.name = "join";
  router.join.input_layers = {"a"_id, "b"_id};
  router.join.add_participant("a"_id);
  router.join.add_participant("b"_id);
  CHECK_THROWS_WITH(
    router.finalize(),
    "The join for 'join' has an ambiguous configuration: input layers [a, b] match paths "
    "[/job/a/b, /job/b/a], but their ancestor relationship differs between those paths.");
}

TEST_CASE("Join completion follows nearest repeated ancestor", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  tbb::flow::queue_node<detail::index_message> parent_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> parent_tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> child_tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/a/a/b"}};
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.name = "placeholder_join";
  router.join.input_layers = {"a"_id, "b"_id};
  router.join.add_participant("a"_id, parent_indices, parent_tokens);
  router.join.add_participant("b"_id, placeholder_indices, child_tokens);
  router.finalize();

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

TEST_CASE("Unfold hierarchy rejects cyclic layer production", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run"}};
  router.provider = {.input_product = {.layer = "run"}, .port = &placeholder_indices};
  std::string expected_error;

  SECTION("Self-cycle")
  {
    router.unfolds = {{.name = "split_run", .input_layers = {"run"_id}, .output_layer = "run"_id}};
    expected_error =
      "Unfold layer hierarchy expansion exceeded max depth 3 (initial deepest 2 + 1 unfold "
      "node(s)). Offending unfold: split_run";
  }

  SECTION("Two-unfold cycle")
  {
    router.unfolds = {
      {.name = "split_spill", .input_layers = {"run"_id}, .output_layer = "spill"_id},
      {.name = "split_run", .input_layers = {"spill"_id}, .output_layer = "run"_id}};
    expected_error =
      "Unfold layer hierarchy expansion exceeded max depth 4 (initial deepest 2 + 2 unfold "
      "node(s)). Offending unfold: split_spill";
  }

  CHECK_THROWS_WITH(router.finalize(), expected_error);
}

TEST_CASE("Unfold hierarchy rejects driver output layer collisions", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  using experimental::layer_path;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  auto const driver_path =
    GENERATE(layer_path{"/job/run/spill"}, layer_path{"/job/run/spill/event"});
  CAPTURE(driver_path);

  router_fixture router{graph};
  router.driver_paths = {driver_path};
  router.unfolds = {{.name = "split", .input_layers = {"run"_id}, .output_layer = "spill"_id}};
  router.provider = {.input_product = {.layer = "run"}, .port = &placeholder_indices};
  CHECK_THROWS_WITH(
    router.finalize(),
    "Unfold 'split' specifies output layer '/job/run/spill' already declared by the driver.");
}

TEST_CASE("Unfold hierarchy rejects duplicate output layers", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  using experimental::layer_path;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};

  auto const driver_path = GENERATE(layer_path{"/job/run"}, layer_path{"/job"});
  CAPTURE(driver_path);

  router_fixture router{graph};
  router.driver_paths = {driver_path};
  router.provider = {.input_product = {.layer = "run"}, .port = &placeholder_indices};
  router.unfolds = {{.name = "split_a", .input_layers = {"run"_id}, .output_layer = "spill"_id},
                    {.name = "split_b", .input_layers = {"run"_id}, .output_layer = "spill"_id}};
  if (driver_path == layer_path{"/job"}) {
    router.unfolds.push_back(
      {.name = "split_run", .input_layers = {"job"_id}, .output_layer = "run"_id});
  }

  CHECK_THROWS_WITH(
    router.finalize(),
    "Unfolds 'split_a' and 'split_b' specify the same output layer '/job/run/spill'.");
}

TEST_CASE("Counting layers on other branches do not produce end tokens", "[graph]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/run/calib"},
                         experimental::layer_path{"/job/spill/event"}};
  router.join.input_layers = {"event"_id};
  router.join.add_participant("run"_id, indices, tokens);
  router.join.add_participant("event"_id, indices, tokens);
  router.finalize();

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
  tbb::flow::queue_node<detail::index_message> placeholder_indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  tbb::flow::queue_node<detail::indexed_end_token> data_tokens{graph};

  router_fixture router{graph};
  router.driver_paths = {experimental::layer_path{"/job/event"},
                         experimental::layer_path{"/job/run/event"}};
  router.provider = {.input_product = {.layer = "job"}, .port = &placeholder_indices};
  router.join.input_layers = {"event"_id};
  router.join.add_participant("job"_id, placeholder_indices, tokens);
  router.join.add_participant("event"_id, placeholder_indices, data_tokens);
  router.finalize();

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

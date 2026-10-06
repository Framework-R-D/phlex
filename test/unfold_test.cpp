// =======================================================================================
// This test executes unfolding functionality using the following graph
//
//     Index Router
//          |
//      unfold (creates children)
//          |
//         add(*)
//          |
//     print_result
//
// where the asterisk (*) indicates a fold step.  The difference here is that the
// *unfold* is responsible for sending the flush token instead of the
// source/index_router.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/core/index_router.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/resource_api.hpp"
#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "phlex/utilities/thread_counter.hpp"
#include "plugins/layer_generator.hpp"
#include "test/ostream_logger.hpp"
#include "test/products_for_output.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <oneapi/tbb/flow_graph.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep
#include <cstddef>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace phlex;
using namespace std::chrono_literals;

namespace {
  class iota {
  public:
    explicit iota(unsigned int max_number) : max_{max_number} {}
    static unsigned int initial_value() { return 0; }
    bool predicate(unsigned int i) const { return i != max_; }
    static auto unfold(unsigned int i) { return std::make_pair(i + 1, i); };

  private:
    unsigned int max_;
  };

  using numbers_t = std::vector<unsigned int>;

  class iterate_through {
  public:
    explicit iterate_through(numbers_t const& numbers) :
      begin_{numbers.begin()}, end_{numbers.end()}
    {
    }
    auto initial_value() const { return begin_; }
    bool predicate(numbers_t::const_iterator it) const { return it != end_; }
    static auto unfold(numbers_t::const_iterator it, data_cell_index const& lid)
    {
      spdlog::info("Unfolding into {}", lid.to_string());
      auto num = *it;
      return std::make_pair(++it, num);
    };

  private:
    numbers_t::const_iterator begin_;
    numbers_t::const_iterator end_;
  };

  void add(std::atomic<unsigned int>& counter, unsigned number) { counter += number; }
  void add_numbers(std::atomic<unsigned int>& counter, unsigned number) { counter += number; }

  void check_sum(handle<unsigned int> const sum)
  {
    if (sum.data_cell_index().number() == 0ull) {
      CHECK(*sum == 45);
    } else {
      CHECK(*sum == 190);
    }
  }

  void check_sum_same(handle<unsigned int> const sum)
  {
    auto const expected_sum = (sum.data_cell_index().number() + 1) * 10;
    CHECK(*sum == expected_sum);
  }

  // Provider algorithms
  unsigned int provide_max_number(data_cell_index const& id) { return 10u * (id.number() + 1); }

  auto provide_ten_numbers(data_cell_index const& id) { return numbers_t(10, id.number() + 1); }

  class iota_two_inputs {
  public:
    iota_two_inputs(unsigned int max_number, numbers_t numbers) :
      max_{max_number}, numbers_{std::move(numbers)}
    {
    }
    static unsigned int initial_value() { return 0; }
    bool predicate(unsigned int i) const { return i != max_ && !numbers_.empty(); }
    static auto unfold(unsigned int i) { return std::make_pair(i + 1, i); };

  private:
    unsigned int max_;
    numbers_t numbers_;
  };

  class iota_with_ancestor : public iota {
  public:
    iota_with_ancestor(unsigned int max_number, unsigned int /*ancestor*/) : iota{max_number} {}
  };

  void check_no_cached_entries(std::ostringstream const& output)
  {
    auto const messages = output.str();
    CHECK_FALSE(messages.contains("Cached"));
    CHECK_FALSE(messages.contains("accumulators"));
    CHECK_FALSE(messages.contains("tracker"));
  }

  // Test load-bearing resource for unfold operations.
  struct unfold_resource {
    using token_type = unfold_resource*;

    detail::thread_counter::counter_type concurrent_unfolds;
  };

  class resource_iota {
  public:
    explicit resource_iota(unsigned int max_number) : max_{max_number} {}

    static unsigned int initial_value() { return 0; }
    bool predicate(unsigned int i) const { return i != max_; }

    static auto unfold(unsigned int i, unfold_resource* resource)
    {
      detail::thread_counter const guard{resource->concurrent_unfolds};
      detail::spin_for(1ms);
      return std::make_pair(i + 1, i);
    }

  private:
    unsigned int max_;
  };
}

TEST_CASE("Splitting the processing", "[graph]")
{
  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [] {
    std::vector<unsigned int> sums1;
    std::vector<unsigned int> sums2;
    constexpr auto number_events = 2u;
    auto gen = experimental::layer_generator::make();
    gen->add_layer("event", {.parent_layer = "job", .count = number_events});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);

    g.provide("provide_max_number", provide_max_number, concurrency::unlimited)
      .output_product("input", "max_number", "event");
    g.provide("provide_ten_numbers", provide_ten_numbers, concurrency::unlimited)
      .output_product("input", "ten_numbers", "event");

    g.unfold<iota>("iota", &iota::predicate, &iota::unfold, concurrency::unlimited, "lower1")
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "max_number"})
      .output_product_suffixes("new_number");
    g.fold("add", add, concurrency::unlimited, "event")
      .input_family(product_selector{.creator = "iota", .layer = "lower1", .suffix = "new_number"})
      .output_product_suffixes("sum1");
    g.observe(
       "check_sum",
       [&sums1](handle<unsigned int> sum) {
         check_sum(sum);
         sums1.push_back(*sum);
       },
       concurrency::serial)
      .input_family(product_selector{.creator = "add", .layer = "event", .suffix = "sum1"});

    g.unfold<iterate_through>("iterate_through",
                              &iterate_through::predicate,
                              &iterate_through::unfold,
                              concurrency::unlimited,
                              "lower2")
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "ten_numbers"})
      .output_product_suffixes("each_number");
    g.fold("add_numbers", add_numbers, concurrency::unlimited, "event")
      .input_family(
        product_selector{.creator = "iterate_through", .layer = "lower2", .suffix = "each_number"})
      .output_product_suffixes("sum2");
    g.observe(
       "check_sum_same",
       [&sums2](handle<unsigned int> sum) {
         check_sum_same(sum);
         sums2.push_back(*sum);
       },
       concurrency::serial)
      .input_family(product_selector{.creator = "add_numbers", .layer = "event", .suffix = "sum2"});

    g.make<experimental::test::products_for_output>().output(
      "save", &experimental::test::products_for_output::save, concurrency::serial);

    g.execute();

    CHECK(g.execution_count("iota") == number_events);
    CHECK(g.execution_count("add") == 30);
    CHECK(g.execution_count("check_sum") == number_events);

    CHECK(g.execution_count("iterate_through") == number_events);
    CHECK(g.execution_count("add_numbers") == 20);
    CHECK(g.execution_count("check_sum_same") == number_events);
    return std::make_pair(std::move(sums1), std::move(sums2));
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto [sums1, sums2] = run_graph();
  std::ranges::sort(sums1);
  std::ranges::sort(sums2);
  CHECK(sums1 == std::vector<unsigned int>{45, 190});
  CHECK(sums2 == std::vector<unsigned int>{10, 20});
  check_no_cached_entries(output);
}

// =======================================================================================
// This test exercises a multi-layer transform whose two inputs come from different data
// layers: one from the unfolded (child) layer and one from the parent (event) layer.
//
/*     Index Router                                                                     */
/*          |                                                                           */
/*     provide_max_number (event layer)                                                 */
/*          |      \                                                                    */
/*     unfold/iota (creates "subevent" children)                                        */
/*          |        \                                                                  */
/*          |         \                                                                 */
/*   (subevent)        (event, repeated)                                                */
/*    new_number        max_number                                                      */
/*          \          /                                                                */
/*      multi-layer transform: max_number + new_number                                  */
// =======================================================================================
TEST_CASE("Multi-layer transform with one input from an unfold", "[graph]")
{
  constexpr auto number_events = 2u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = number_events});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);

  g.provide("provide_max_number", provide_max_number, concurrency::unlimited)
    .output_product("input", "max_number", "event");

  g.unfold<iota>("iota", &iota::predicate, &iota::unfold, concurrency::unlimited, "subevent")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "max_number"})
    .output_product_suffixes("new_number");

  g.transform(
     "add_max_and_new",
     [](unsigned int i, unsigned int j) { return i + j; },
     concurrency::unlimited)
    .input_family(product_selector{.creator = "iota", .layer = "subevent", .suffix = "new_number"},
                  product_selector{.creator = "input", .layer = "event", .suffix = "max_number"})
    .output_product_suffixes("result");

  g.execute();

  // event 0: max_number=10, new_number in [0,9]  -> 10 executions
  // event 1: max_number=20, new_number in [0,19] -> 20 executions
  CHECK(g.execution_count("iota") == number_events);
  CHECK(g.execution_count("add_max_and_new") == 30u);
}

TEST_CASE("Unfold deduplicates same-layer inputs for bookkeeping", "[graph]")
{
  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [] {
    std::vector<unsigned int> sums;
    constexpr auto number_events = 2u;
    auto gen = experimental::layer_generator::make();
    gen->add_layer("event", {.parent_layer = "job", .count = number_events});

    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);

    g.provide("provide_max_number", provide_max_number, concurrency::unlimited)
      .output_product("input", "max_number", "event");
    g.provide("provide_ten_numbers", provide_ten_numbers, concurrency::unlimited)
      .output_product("input", "ten_numbers", "event");

    g.unfold<iota_two_inputs>("iota_two_inputs",
                              &iota_two_inputs::predicate,
                              &iota_two_inputs::unfold,
                              concurrency::unlimited,
                              "subevent")
      .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "max_number"},
                    product_selector{.creator = "input", .layer = "event", .suffix = "ten_numbers"})
      .output_product_suffixes("new_number");

    g.fold("add_dual_input_unfold", add, concurrency::unlimited, "event")
      .input_family(
        product_selector{.creator = "iota_two_inputs", .layer = "subevent", .suffix = "new_number"})
      .output_product_suffixes("sum");

    g.observe(
       "check_dual_input_unfold_sum",
       [&sums](unsigned int sum) { sums.push_back(sum); },
       concurrency::serial)
      .input_family(
        product_selector{.creator = "add_dual_input_unfold", .layer = "event", .suffix = "sum"});

    g.execute();

    CHECK(g.execution_count("iota_two_inputs") == number_events);
    CHECK(g.execution_count("add_dual_input_unfold") == 30u);
    CHECK(g.execution_count("check_dual_input_unfold_sum") == number_events);
    return sums;
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto sums = run_graph();
  std::ranges::sort(sums);
  CHECK(sums == std::vector<unsigned int>{45, 190});
  check_no_cached_entries(output);
}

TEST_CASE("Unfold receives a resource token", "[graph][unfold][resource]")
{
  constexpr auto num_events = 2u;

  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = num_events});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_serialized_resource<unfold_resource>();

  g.provide(
     "provide_max_number", [](data_cell_index const&) { return 10u; }, concurrency::unlimited)
    .output_product("input", "max_number", "event");

  g.unfold<resource_iota>("resource_iota",
                          &resource_iota::predicate,
                          &resource_iota::unfold,
                          concurrency::unlimited,
                          "subevent")
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "max_number"},
                  resource<unfold_resource>{})
    .output_product_suffixes("new_number");

  g.execute();

  CHECK(g.execution_count("resource_iota") == num_events);
}

// Reproducer for issue #955: job -> spill --split_a--> a --split_b--> b.
// Joining the spill product in split_b must not prevent the spill fold from flushing.
TEST_CASE("Nested unfold with a higher-layer input flushes the outer layer", "[graph][unfold]")
{
  using data_cell_index_number = unsigned int;
  using counts_t = std::vector<std::pair<data_cell_index_number, int>>;
  struct scenario {
    char const* description;
    std::vector<unsigned int> expected_b_counts;
  };
  auto const test =
    GENERATE(scenario{.description = "Two-input nested unfold (issue reproducer)",
                      .expected_b_counts = {2u, 2u, 2u}},
             scenario{.description = "Some empty b children, including an empty spill partition",
                      .expected_b_counts = {0u, 1u, 2u}},
             scenario{.description = "All b partitions empty", .expected_b_counts = {0u, 0u, 0u}});
  CAPTURE(test.description);

  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [](scenario const& test) {
    counts_t counts;
    counts_t sums;
    auto const num_spills = static_cast<unsigned int>(test.expected_b_counts.size());
    constexpr int threads = 1;
    auto const num_b =
      std::accumulate(test.expected_b_counts.begin(), test.expected_b_counts.end(), std::size_t{0});

    auto gen = experimental::layer_generator::make();
    gen->add_layer("spill", {.parent_layer = "job", .count = num_spills});

    auto g = phlex::detail::framework_graph::without_driver("test", threads);
    g.add_driver(gen);
    g.provide(
       "provide_n", [](data_cell_index const&) { return 2u; }, concurrency::unlimited)
      .output_product("src", "n", "spill");

    product_selector const spill_n{.creator = "src", .layer = "spill", .suffix = "n"};
    product_selector const a_n{.creator = "split_a", .layer = "a", .suffix = "n"};
    product_selector const b_n{.creator = "split_b", .layer = "b", .suffix = "n"};
    product_selector const split_b_input{
      .creator = "set_b_child_count", .layer = "a", .suffix = "n"};

    g.transform(
       "set_b_child_count",
       [expected_b_counts = test.expected_b_counts](handle<unsigned int> n) {
         auto const& index = n.data_cell_index();
         auto const spill_number = index.parent()->number();
         return static_cast<unsigned int>(index.number() < expected_b_counts.at(spill_number));
       },
       concurrency::unlimited)
      .input_family(a_n)
      .output_product_suffixes("n");

    g.unfold<iota>("split_a", &iota::predicate, &iota::unfold, concurrency::unlimited, "a")
      .input_family(spill_n)
      .output_product_suffixes("n");

    g.unfold<iota_with_ancestor>(
       "split_b", &iota::predicate, &iota::unfold, concurrency::unlimited, "b")
      .input_family(split_b_input, spill_n)
      .output_product_suffixes("n");

    g.fold(
       "count", [](int& count, unsigned int) { ++count; }, concurrency::serial, "spill")
      .input_family(b_n)
      .output_product_suffixes("count");
    g.observe(
       "check_count",
       [&counts](handle<int> count) {
         counts.emplace_back(count.data_cell_index().number(), *count);
       },
       concurrency::serial)
      .input_family(product_selector{.creator = "count", .layer = "spill", .suffix = "count"});

    g.transform(
       "join_b_spill",
       [](unsigned int b, unsigned int spill) { return static_cast<int>(b + spill); },
       concurrency::unlimited)
      .input_family(b_n, spill_n)
      .output_product_suffixes("n");
    g.fold(
       "sum_joined", [](int& sum, int n) { sum += n; }, concurrency::serial, "spill")
      .input_family(product_selector{.creator = "join_b_spill", .layer = "b", .suffix = "n"})
      .output_product_suffixes("sum");
    g.observe(
       "check_joined",
       [&sums](handle<int> sum) { sums.emplace_back(sum.data_cell_index().number(), *sum); },
       concurrency::serial)
      .input_family(product_selector{.creator = "sum_joined", .layer = "spill", .suffix = "sum"});

    g.execute();

    CHECK(g.execution_count("split_a") == num_spills);
    CHECK(g.execution_count("split_b") == static_cast<std::size_t>(2 * num_spills));
    CHECK(g.execution_count("count") == num_b);
    CHECK(g.execution_count("join_b_spill") == num_b);
    CHECK(g.execution_count("sum_joined") == num_b);
    // Check emission separately: checks inside the observer cannot catch it never running.
    CHECK(g.execution_count("check_count") == num_spills);
    CHECK(g.execution_count("check_joined") == num_spills);
    return std::make_pair(std::move(counts), std::move(sums));
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto [counts, sums] = run_graph(test);
  std::ranges::sort(counts);
  std::ranges::sort(sums);
  counts_t expected_counts;
  counts_t expected_sums;
  for (data_cell_index_number spill = 0; spill != test.expected_b_counts.size(); ++spill) {
    auto const count = static_cast<int>(test.expected_b_counts[spill]);
    expected_counts.emplace_back(spill, count);
    // Each b product is 0, and the spill product is 2.
    expected_sums.emplace_back(spill, count * 2);
  }
  CHECK(counts == expected_counts);
  CHECK(sums == expected_sums);
  check_no_cached_entries(output);
}

TEST_CASE("Unfold hierarchy resolves dependencies before their producers", "[graph][unfold]")
{
  using namespace phlex::experimental::literals;
  tbb::flow::graph graph;
  tbb::flow::queue_node<detail::index_message> indices{graph};
  tbb::flow::queue_node<detail::indexed_end_token> tokens{graph};
  detail::index_router router{graph};
  router.finalize(
    graph,
    {experimental::layer_path{"/job/spill"}},
    // The dependent comes first both in this list and in lexical node-name order.
    {{.name = "a_split_b", .input_layers = {"a"_id, "spill"_id}, .output_layer = "b"_id},
     {.name = "z_split_a", .input_layers = {"spill"_id}, .output_layer = "a"_id}},
    {{"dummy", {.input_product = {.layer = "spill"}, .port = &indices}}},
    {},
    {{"fold",
      {{.layer = "spill"_id,
        .counting_layer = "b"_id,
        .token_port = &tokens,
        .index_port = &indices}}}});

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
  CHECK_FALSE(tokens.try_get(token));

  router.unfold_flush_receiver().try_put({.index = a, .layer_hash = b->layer_hash(), .count = 3});
  graph.wait_for_all();
  REQUIRE(tokens.try_get(token));
  CHECK(token.index == spill);
  CHECK(token.count == 3);
  CHECK_FALSE(tokens.try_get(token));
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
      "node(s)). Offending unfold(s):\n  - split_run";
  }

  SECTION("Two-unfold cycle")
  {
    unfolds = {{.name = "split_spill", .input_layers = {"run"_id}, .output_layer = "spill"_id},
               {.name = "split_run", .input_layers = {"spill"_id}, .output_layer = "run"_id}};
    expected_error =
      "Unfold layer hierarchy expansion exceeded max depth 4 (initial deepest 2 + 2 unfold "
      "node(s)). Offending unfold(s):\n  - split_spill";
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

TEST_CASE("Unfold flush expectations distinguish repeated layer names", "[graph][unfold]")
{
  std::ostringstream output;
  auto logger = test::use_ostream_logger(output);
  auto const run_graph = [] {
    std::vector<std::pair<std::string, int>> counts;
    std::vector<int> extra_counts;
    auto gen = experimental::layer_generator::make();
    gen->add_layer("run", {.parent_layer = "job", .count = 1});
    gen->add_layer("spill", {.parent_layer = "run", .count = 2});
    gen->add_layer("spill", {.parent_layer = "job", .count = 3});
    auto g = phlex::detail::framework_graph::without_driver("test");
    g.add_driver(gen);
    g.provide(
       "provide_n",
       [](data_cell_index const& index) { return index.depth() == 1 ? 2u : 3u; },
       concurrency::unlimited)
      .output_product("src", "n", "spill");
    g.provide(
       "provide_run", [](data_cell_index const&) { return 7u; }, concurrency::unlimited)
      .output_product("src", "n", "run");

    product_selector const spill_n{.creator = "src", .layer = "spill", .suffix = "n"};
    g.unfold<iota>("split_a", &iota::predicate, &iota::unfold, concurrency::unlimited, "a")
      .input_family(spill_n)
      .output_product_suffixes("n");
    // Only /job/run/spill has a run ancestor, so its flush gate expects a second unfold.
    g.unfold<iota_with_ancestor>(
       "split_c", &iota::predicate, &iota::unfold, concurrency::unlimited, "c")
      .input_family(spill_n, product_selector{.creator = "src", .layer = "run", .suffix = "n"})
      .output_product_suffixes("n");
    g.fold(
       "count_a", [](int& count, unsigned int) { ++count; }, concurrency::serial, "spill")
      .input_family(product_selector{.creator = "split_a", .layer = "a", .suffix = "n"})
      .output_product_suffixes("count");
    g.observe(
       "check_a",
       [&counts](handle<int> count) {
         counts.emplace_back(count.data_cell_index().layer_path().to_string(), *count);
       },
       concurrency::serial)
      .input_family(product_selector{.creator = "count_a", .layer = "spill", .suffix = "count"});
    g.fold(
       "count_c", [](int& count, unsigned int) { ++count; }, concurrency::serial, "run")
      .input_family(product_selector{.creator = "split_c", .layer = "c", .suffix = "n"})
      .output_product_suffixes("count");
    g.observe(
       "check_c",
       [&extra_counts](int count) { extra_counts.push_back(count); },
       concurrency::serial)
      .input_family(product_selector{.creator = "count_c", .layer = "run", .suffix = "count"});

    g.execute();
    CHECK(g.execution_count("split_a") == 5);
    CHECK(g.execution_count("split_c") == 2);
    CHECK(g.execution_count("count_a") == 12);
    CHECK(g.execution_count("count_c") == 6);
    CHECK(g.execution_count("check_a") == 5);
    CHECK(g.execution_count("check_c") == 1);
    return std::make_pair(std::move(counts), std::move(extra_counts));
  };
  // Destroy the graph before checking for cached-entry warnings.
  auto [counts, extra_counts] = run_graph();
  std::ranges::sort(counts);
  CHECK(counts == std::vector<std::pair<std::string, int>>{{"/job/run/spill", 3},
                                                           {"/job/run/spill", 3},
                                                           {"/job/spill", 2},
                                                           {"/job/spill", 2},
                                                           {"/job/spill", 2}});
  CHECK(extra_counts == std::vector<int>{6});
  check_no_cached_entries(output);
}

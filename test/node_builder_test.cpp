#include "phlex/core/node_builder.hpp"
#include "phlex/utilities/sleep_for.hpp"

#include "phlex/concurrency.hpp"

#include "catch2/catch_test_macros.hpp"
#include "oneapi/tbb/parallel_for.h"

#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <tuple>

using namespace phlex;
using namespace phlex::detail;
using namespace std::chrono_literals;

namespace {
  struct unlimited_resource_type {};

  struct single_token_resource_type {
    using token_type = single_token_resource_type const*;
  };

  struct value_token_resource_type {
    using token_type = value_token_resource_type;

    explicit value_token_resource_type(int v) : value(v) {}

    int value;
  };

  // The production node bodies adapt product messages and output ports before invoking the
  // registered algorithm. These tests use an int input, so direct invocation isolates resource
  // argument assembly from node-family-specific behavior.
  auto const invoke_operation =
    [](auto const& operation, auto const& value, auto const&... resources) {
      std::invoke(operation, value, resources...);
    };

  // Records how many node-body invocations ran and how many ever ran at once. Catch2
  // assertions are not thread-safe, so the bodies only accumulate into these atomics and the
  // test thread asserts once the graph has completed.
  class concurrency_observations {
  public:
    using value_type = unsigned int;

    // Marks the extent of one invocation, so the active count is decremented even if the
    // node body exits early.
    class scope {
    public:
      explicit scope(concurrency_observations& observations) : observations_{observations}
      {
        observations_.enter();
      }

      ~scope() { observations_.leave(); }

      scope(scope const&) = delete;
      scope& operator=(scope const&) = delete;
      scope(scope&&) = delete;
      scope& operator=(scope&&) = delete;

    private:
      // Non-owning reference to externally-owned observations; scope is an RAII guard.
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
      concurrency_observations& observations_;
    };

    value_type calls() const noexcept { return calls_; }
    value_type maximum_active() const noexcept { return maximum_active_; }

  private:
    // std::atomic::fetch_max is C++26 (P0493); until the supported toolchains provide it, fall
    // back to a compare-exchange loop. Both forms are a single atomic maximum update, so the
    // observable behavior is identical.
    static void record_maximum(std::atomic<value_type>& value, value_type candidate) noexcept
    {
#ifdef __cpp_lib_atomic_min_max
      value.fetch_max(candidate);
#else
      auto previous = value.load();
      while (previous < candidate && !value.compare_exchange_weak(previous, candidate)) {}
#endif
    }

    void enter() noexcept
    {
      ++calls_;
      record_maximum(maximum_active_, ++active_);
    }

    void leave() noexcept { --active_; }

    std::atomic<value_type> active_;
    std::atomic<value_type> maximum_active_;
    std::atomic<value_type> calls_;
  };

  constexpr auto number_of_messages = 64uz;

  static_assert(
    std::same_as<node_type_for_t<int, no_outputs_t, std::tuple<unlimited_resource_type>>,
                 tbb::flow::function_node<int>>);
  static_assert(
    std::same_as<node_type_for_t<int, std::tuple<int>, std::tuple<unlimited_resource_type>>,
                 tbb::flow::function_node<int, int>>);
  static_assert(
    std::same_as<
      node_type_for_t<int, multifunction_outputs<int>, std::tuple<unlimited_resource_type>>,
      tbb::flow::multifunction_node<int, std::tuple<int>>>);
  static_assert(
    std::same_as<node_type_for_t<int, no_outputs_t, std::tuple<single_token_resource_type>>,
                 tbb::flow::resource_limited_node<int, no_outputs_t>>);
}

TEST_CASE("node builder supplies unlimited resources", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<unlimited_resource_type>();
  std::atomic<bool> called{};

  using function_t = std::function<void(int, unlimited_resource_type const*)>;
  using builder_t =
    node_builder<int, function_t, no_outputs_t, std::tuple<unlimited_resource_type>>;
  auto node = builder_t::make(graph,
                              concurrency::serial.value,
                              resources,
                              function_t{[&called](int value, unlimited_resource_type const*) {
                                CHECK(value == 42);
                                called = true;
                              }},
                              invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves unlimited then bounded resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<unlimited_resource_type>();
  resources.add<single_token_resource_type>();
  std::atomic<bool> called{};

  using function_t =
    std::function<void(int, unlimited_resource_type const*, single_token_resource_type const*)>;
  using builder_t = node_builder<int,
                                 function_t,
                                 no_outputs_t,
                                 std::tuple<unlimited_resource_type, single_token_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    function_t{
      [&called](int value, unlimited_resource_type const*, single_token_resource_type const*) {
        CHECK(value == 42);
        called = true;
      }},
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves bounded then unlimited resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<unlimited_resource_type>();
  resources.add<single_token_resource_type>();
  std::atomic<bool> called{};

  using function_t =
    std::function<void(int, single_token_resource_type const*, unlimited_resource_type const*)>;
  using builder_t = node_builder<int,
                                 function_t,
                                 no_outputs_t,
                                 std::tuple<single_token_resource_type, unlimited_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    function_t{
      [&called](int value, single_token_resource_type const*, unlimited_resource_type const*) {
        CHECK(value == 42);
        called = true;
      }},
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies value resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<value_token_resource_type>(42);
  std::atomic<bool> called{};

  using function_t = std::function<void(int, value_token_resource_type)>;
  using builder_t =
    node_builder<int, function_t, no_outputs_t, std::tuple<value_token_resource_type>>;
  auto node = builder_t::make(graph,
                              concurrency::serial.value,
                              resources,
                              function_t{[&called](int value, value_token_resource_type token) {
                                CHECK(value == 42);
                                CHECK(token.value == 42);
                                called = true;
                              }},
                              invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("a shared single-token resource limits concurrency",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<single_token_resource_type>();
  concurrency_observations observations;

  using function_t = std::function<void(int, single_token_resource_type const*)>;
  using builder_t =
    node_builder<int, function_t, no_outputs_t, std::tuple<single_token_resource_type>>;
  auto function = function_t{[&observations](int, single_token_resource_type const*) {
    concurrency_observations::scope const invocation{observations};
    spin_for(1ms);
  }};
  auto node1 =
    builder_t::make(graph, concurrency::unlimited.value, resources, function, invoke_operation);
  auto node2 =
    builder_t::make(graph, concurrency::unlimited.value, resources, function, invoke_operation);

  // Submitting concurrently keeps both nodes contending for the single token; feeding them
  // serially would let each invocation complete before the next message arrived.
  tbb::parallel_for(0uz, number_of_messages, [&](std::size_t value) {
    node1.try_put(static_cast<int>(value));
    node2.try_put(static_cast<int>(value));
  });
  graph.wait_for_all();

  CHECK(observations.calls() == 2 * number_of_messages);
  CHECK(observations.maximum_active() == 1u);
}

TEST_CASE("mixed resources are limited by the bounded resource",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add<unlimited_resource_type>();
  resources.add<single_token_resource_type>();
  concurrency_observations observations;

  using function_t =
    std::function<void(int, unlimited_resource_type const*, single_token_resource_type const*)>;
  using builder_t = node_builder<int,
                                 function_t,
                                 no_outputs_t,
                                 std::tuple<unlimited_resource_type, single_token_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::unlimited.value,
    resources,
    function_t{
      [&observations](int, unlimited_resource_type const*, single_token_resource_type const*) {
        concurrency_observations::scope const invocation{observations};
        spin_for(1ms);
      }},
    invoke_operation);

  // Submitting concurrently keeps the node contending for the single token; feeding it
  // serially would let each invocation complete before the next message arrived.
  tbb::parallel_for(
    0uz, number_of_messages, [&](std::size_t value) { node.try_put(static_cast<int>(value)); });
  graph.wait_for_all();

  CHECK(observations.calls() == number_of_messages);
  CHECK(observations.maximum_active() == 1u);
}

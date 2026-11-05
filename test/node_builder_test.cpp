#include "phlex/concurrency.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/resource/catalog.hpp"
#include "phlex/utilities/sleep_for.hpp"

#include <catch2/catch_test_macros.hpp>
#include <oneapi/tbb/flow_graph.h>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <array>
#include <chrono> // IWYU pragma: keep
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <span>
#include <tuple>
#include <vector>

using namespace phlex;
using namespace phlex::detail;
using namespace std::chrono_literals;

namespace {
  struct unlimited_resource_type {};

  struct serialized_resource_type {
    using token_type = serialized_resource_type const*;
  };

  struct value_token_resource_type {
    using token_type = value_token_resource_type;

    explicit value_token_resource_type(int v) : value(v) {}

    int value;
  };

  struct custom_token_resource_type {
    using token_type = int;

    static int token() { return 42; }
  };

  struct pooled_resource_type {
    static std::vector<int> tokens() { return {42, 84}; }
  };

  struct one_token_pooled_resource_type {
    static std::array<int, 1> tokens() { return {42}; }
  };

  struct span_pooled_resource_type {
    std::array<int, 2> values{101, 202};

    std::span<int const> tokens() const { return values; }
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

  // Releases two invocations together, but lets the test finish within a fixed bound if the
  // pool or scheduler fails to provide overlap.
  class bounded_overlap_gate {
  public:
    void arrive_and_wait()
    {
      std::unique_lock lock{mutex_};
      if (arrivals_ >= 2) {
        return;
      }
      ++arrivals_;
      condition_.notify_all();
      if (!condition_.wait_for(lock, 5s, [this] { return arrivals_ >= 2; })) {
        timed_out_ = true;
      }
    }

    bool released_two_together() const
    {
      std::scoped_lock const lock{mutex_};
      return arrivals_ >= 2 && !timed_out_;
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    unsigned int arrivals_{};
    bool timed_out_{};
  };

  constexpr auto number_of_messages = 64uz;

  // Zero-resource node shapes.
  static_assert(std::same_as<node_builder<int, no_outputs_t, std::tuple<>>::node_t,
                             tbb::flow::function_node<int>>);
  static_assert(std::same_as<node_builder<int, std::tuple<int>, std::tuple<>>::node_t,
                             tbb::flow::function_node<int, int>>);
  static_assert(std::same_as<node_builder<int, multifunction_outputs<int>, std::tuple<>>::node_t,
                             tbb::flow::multifunction_node<int, std::tuple<int>>>);

  // Resource-dependent node selection.
  static_assert(
    std::same_as<node_builder<int, no_outputs_t, std::tuple<unlimited_resource_type>>::node_t,
                 tbb::flow::function_node<int>>);
  static_assert(
    std::same_as<node_builder<int, std::tuple<int>, std::tuple<unlimited_resource_type>>::node_t,
                 tbb::flow::function_node<int, int>>);
  static_assert(
    std::same_as<
      node_builder<int, multifunction_outputs<int>, std::tuple<unlimited_resource_type>>::node_t,
      tbb::flow::multifunction_node<int, std::tuple<int>>>);
  static_assert(
    std::same_as<node_builder<int, no_outputs_t, std::tuple<serialized_resource_type>>::node_t,
                 tbb::flow::resource_limited_node<int, no_outputs_t>>);
  static_assert(
    std::same_as<node_builder<int, no_outputs_t, std::tuple<pooled_resource_type>>::node_t,
                 tbb::flow::resource_limited_node<int, no_outputs_t>>);
}

TEST_CASE("node builder supplies unlimited resources", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_resource_type>();
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<unlimited_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, unlimited_resource_type const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves unlimited then serialized resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_resource_type>();
  resources.add_serialized<serialized_resource_type>();
  std::atomic<bool> called{};

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<unlimited_resource_type, serialized_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, unlimited_resource_type const*, serialized_resource_type const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder preserves serialized then unlimited resource order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_resource_type>();
  resources.add_serialized<serialized_resource_type>();
  std::atomic<bool> called{};

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<serialized_resource_type, unlimited_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, serialized_resource_type const*, unlimited_resource_type const*) {
      CHECK(value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(42));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies value resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<value_token_resource_type>(42);
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<value_token_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, value_token_resource_type token) {
      CHECK(value == 43);
      CHECK(token.value == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies custom resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<custom_token_resource_type>();
  std::atomic<bool> called{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<custom_token_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&called](int value, int token) {
      CHECK(value == 43);
      CHECK(token == 42);
      called = true;
    },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();
  CHECK(called);
}

TEST_CASE("node builder supplies pooled resource tokens", "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_pooled<pooled_resource_type>();
  std::atomic<int> received_token{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<pooled_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&received_token](int, int token) { received_token = token; },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();
  auto const token = received_token.load();
  CHECK((token == 42 || token == 84));
}

TEST_CASE("pooled span values remain valid and distinct across dependent nodes",
          "[node_builder][resource][concurrency]")
{
  tbb::global_control parallelism{tbb::global_control::max_allowed_parallelism, 4};
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_pooled<span_pooled_resource_type>();
  concurrency_observations observations;
  bounded_overlap_gate gate;
  std::atomic<unsigned int> token_values{};
  std::atomic<unsigned int> accepted_messages{};

  using builder_t = node_builder<int, no_outputs_t, std::tuple<span_pooled_resource_type>>;
  auto function = [&observations, &gate, &token_values](int, int const token) {
    concurrency_observations::scope const invocation{observations};
    if (token == 101) {
      token_values.fetch_or(1u);
    } else if (token == 202) {
      token_values.fetch_or(2u);
    }
    gate.arrive_and_wait();
  };
  auto node1 =
    builder_t::make(graph, concurrency::unlimited.value, resources, function, invoke_operation);
  auto node2 =
    builder_t::make(graph, concurrency::unlimited.value, resources, function, invoke_operation);

  tbb::parallel_for(0uz, 2uz, [&](std::size_t value) {
    accepted_messages += node1.try_put(static_cast<int>(value));
    accepted_messages += node2.try_put(static_cast<int>(value));
  });
  graph.wait_for_all();

  CHECK(accepted_messages == 4u);
  CHECK(observations.calls() == 4u);
  CHECK(observations.maximum_active() == 2u);
  CHECK(gate.released_two_together());
  CHECK(token_values == 3u);
}

TEST_CASE("one-token pooled resource limits dependent invocations",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_pooled<one_token_pooled_resource_type>();
  concurrency_observations observations;

  using builder_t = node_builder<int, no_outputs_t, std::tuple<one_token_pooled_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::unlimited.value,
    resources,
    [&observations](int, int token) {
      concurrency_observations::scope const invocation{observations};
      if (token != 42) {
        return;
      }
      spin_for(1ms);
    },
    invoke_operation);

  tbb::parallel_for(
    0uz, number_of_messages, [&](std::size_t value) { node.try_put(static_cast<int>(value)); });
  graph.wait_for_all();

  CHECK(observations.calls() == number_of_messages);
  CHECK(observations.maximum_active() == 1u);
}

TEST_CASE("node concurrency remains below pooled capacity", "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_pooled<pooled_resource_type>();
  concurrency_observations observations;

  using builder_t = node_builder<int, no_outputs_t, std::tuple<pooled_resource_type>>;
  auto node = builder_t::make(
    graph,
    1,
    resources,
    [&observations](int, int) {
      concurrency_observations::scope const invocation{observations};
      spin_for(1ms);
    },
    invoke_operation);

  tbb::parallel_for(
    0uz, number_of_messages, [&](std::size_t value) { node.try_put(static_cast<int>(value)); });
  graph.wait_for_all();

  CHECK(observations.calls() == number_of_messages);
  CHECK(observations.maximum_active() == 1u);
}

TEST_CASE("node builder preserves mixed unlimited serialized and pooled argument order",
          "[node_builder][resource]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_resource_type>();
  resources.add_serialized<serialized_resource_type>();
  resources.add_pooled<pooled_resource_type>();
  std::atomic<bool> received_expected_arguments{};

  using builder_t = node_builder<
    int,
    no_outputs_t,
    std::tuple<pooled_resource_type, unlimited_resource_type, serialized_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::serial.value,
    resources,
    [&received_expected_arguments](
      int, int pooled_token, unlimited_resource_type const*, serialized_resource_type const*) {
      received_expected_arguments = pooled_token == 42 || pooled_token == 84;
    },
    invoke_operation);
  REQUIRE(node.try_put(43));
  graph.wait_for_all();

  CHECK(received_expected_arguments);
}

TEST_CASE("a shared serialized resource limits concurrency",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<serialized_resource_type>();
  concurrency_observations observations;

  using builder_t = node_builder<int, no_outputs_t, std::tuple<serialized_resource_type>>;
  auto function = [&observations](int, serialized_resource_type const*) {
    concurrency_observations::scope const invocation{observations};
    spin_for(1ms);
  };
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

TEST_CASE("mixed resources are limited by the serialized resource",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_unlimited<unlimited_resource_type>();
  resources.add_serialized<serialized_resource_type>();
  concurrency_observations observations;

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<unlimited_resource_type, serialized_resource_type>>;
  auto node = builder_t::make(
    graph,
    concurrency::unlimited.value,
    resources,
    [&observations](int, unlimited_resource_type const*, serialized_resource_type const*) {
      concurrency_observations::scope const invocation{observations};
      spin_for(1ms);
    },
    invoke_operation);

  // Submitting concurrently keeps the node contending for the single token; feeding it
  // serially would let each invocation complete before the next message arrived.
  tbb::parallel_for(
    0uz, number_of_messages, [&](std::size_t value) { node.try_put(static_cast<int>(value)); });
  graph.wait_for_all();

  CHECK(observations.calls() == number_of_messages);
  CHECK(observations.maximum_active() == 1u);
}

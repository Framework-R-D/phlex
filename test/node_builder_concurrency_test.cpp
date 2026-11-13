#include "phlex/concurrency.hpp"
#include "phlex/core/node_builder.hpp"
#include "phlex/core/resource/catalog.hpp"
#include "phlex/utilities/sleep_for.hpp"
#include "test/node_builder_helpers.hpp"
#include "test/resources_for_testing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <oneapi/tbb/flow_graph.h>
#include <oneapi/tbb/parallel_for.h>

#include <atomic>
// Provides chrono literals used below but attributed to a transitive include by include-cleaner.
#include <chrono> // IWYU pragma: keep
#include <cstddef>
#include <tuple>

using namespace phlex;
using namespace phlex::detail;
using namespace phlex::test;
using namespace std::chrono_literals;

namespace {
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
}

TEST_CASE("a shared serialized resource limits concurrency",
          "[node_builder][resource][concurrency]")
{
  tbb::flow::graph graph;
  resource_catalog resources;
  resources.add_serialized<pointer_token_resource>();
  concurrency_observations observations;

  using builder_t = node_builder<int, no_outputs_t, std::tuple<pointer_token_resource>>;
  auto function = [&observations](int, pointer_token_resource const*) {
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
  resources.add_unlimited<unlimited_test_resource>();
  resources.add_serialized<pointer_token_resource>();
  concurrency_observations observations;

  using builder_t =
    node_builder<int, no_outputs_t, std::tuple<unlimited_test_resource, pointer_token_resource>>;
  auto node = builder_t::make(
    graph,
    concurrency::unlimited.value,
    resources,
    [&observations](int, unlimited_test_resource const*, pointer_token_resource const*) {
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

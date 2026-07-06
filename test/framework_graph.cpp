#include "phlex/core/framework_graph.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "boost/core/demangle.hpp"
#include "fmt/format.h"

#include <functional>
#include <stdexcept>
#include <typeinfo>
#include <vector>

using namespace phlex;
using phlex::detail::driver_bundle;
using phlex::detail::framework_driver;

namespace {
  struct test_source final : phlex::source {
    phlex::detail::provider_bundles create_providers(product_selector const&) override
    {
      return {};
    }
    index_generator indices() override { co_return; }
  };

  struct other_source final : phlex::source {
    phlex::detail::provider_bundles create_providers(product_selector const&) override
    {
      return {};
    }
    index_generator indices() override { co_return; }
  };

  struct test_driver_builder {
    [[nodiscard]] fixed_hierarchy hierarchy() const { return {}; }

    [[nodiscard]] std::function<void(data_cell_yielder const)> driver_function() const
    {
      return [](data_cell_yielder const /*yielder*/) {};
    }
  };
}

TEST_CASE("Catch STL exceptions", "[graph]")
{
  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(driver_bundle{[](framework_driver&) { throw std::runtime_error("STL error"); }, {}});
  CHECK_THROWS_AS(g.execute(), std::exception);
}

TEST_CASE("Catch other exceptions", "[graph]")
{
  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(driver_bundle{[](framework_driver&) { throw 2.5; }, {}});
  CHECK_THROWS_AS(g.execute(), double);
}

TEST_CASE("Make progress with one thread", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("spill", {"job", 1000});

  auto g = phlex::detail::framework_graph::without_driver(1);
  g.add_driver(gen);
  g.provide(
     "provide_number",
     [](data_cell_index const& index) -> unsigned int { return index.number(); },
     concurrency::unlimited)
    .output_product("input", "number", "spill");
  g.observe(
     "observe_number", [](unsigned int const /*number*/) {}, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "spill", .suffix = "number"});
  g.execute();

  CHECK(gen->emitted_cell_count("/job/spill") == 1000);
  CHECK(g.execution_count("provide_number") == 1000);
  CHECK(g.execution_count("observe_number") == 1000);
}

TEST_CASE("Stop driver when workflow throws exception", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("spill", {"job", 1000});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);
  g.provide(
     "throw_exception",
     [](data_cell_index const&) -> unsigned int {
       throw std::runtime_error("Error to stop driver");
     },
     concurrency::unlimited)
    .output_product("input", "number", "spill");

  // Must have at least one downstream node that requires something of the
  // provider...otherwise provider will not be executed.
  g.observe(
     "downstream_of_exception", [](unsigned int) {}, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "spill", .suffix = "number"});

  CHECK_THROWS(g.execute());

  // The framework will see no more data cells than what the driver emits.
  //
  // With the current implementation, it is possible that framework graph will not see the
  // "/job/spill" data layer before the job ends.  In that case, the "/job/spill" layer
  // will not have been recorded, and we therefore allow it to be "missing", which is what
  // the 'true' argument allows for.
  CHECK(gen->emitted_cell_count("/job/spill") >= g.seen_cell_count("/job/spill", true));

  // A node has not "executed" until it has returned successfully.  For that reason,
  // neither the "throw_exception" provider nor the "downstream_of_exception" observer
  // will have executed.
  CHECK(g.execution_count("throw_exception") == 0ull);
  CHECK(g.execution_count("downstream_of_exception") == 0ull);
}

TEST_CASE("Throw when predicate specified by consumer does not exist", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {"job", 1, 1});

  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);
  g.provide(
     "provide_num",
     [](data_cell_index const& id) -> unsigned int { return id.number(); },
     concurrency::unlimited)
    .output_product("input", "num", "event");

  g.observe(
     "observe_num", [](unsigned int const) {}, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "num"})
    .experimental_when("missing_predicate");

  CHECK_THROWS_WITH(
    g.execute(),
    Catch::Matchers::ContainsSubstring(
      "A non-existent filter with the name 'missing_predicate' was specified for observe_num"));
}

TEST_CASE("Throw for invalid deferred driver setup", "[graph]")
{
  auto g = phlex::detail::framework_graph::without_driver();

  SECTION("Throw when source specified for driver does not exist")
  {
    CHECK_THROWS_WITH(
      g.driver_proxy({"missing_source"}),
      Catch::Matchers::ContainsSubstring("Unknown source with name: missing_source"));
  }

  SECTION("Throw when no driver configured")
  {
    CHECK_THROWS_WITH(
      g.execute(), Catch::Matchers::ContainsSubstring("No driver configured for framework_graph"));
  }

  SECTION("Throw when configuring empty driver bundle")
  {
    CHECK_THROWS_WITH(
      g.add_driver(driver_bundle{}),
      Catch::Matchers::ContainsSubstring("Cannot configure framework_graph with an empty driver."));
  }
}

TEST_CASE("Use default driver", "[graph]")
{
  auto g = phlex::detail::framework_graph::with_default_driver();

  SECTION("Throw when attempting to add a driver in default mode")
  {
    CHECK_THROWS_WITH(
      g.add_driver(driver_bundle{}),
      Catch::Matchers::ContainsSubstring(
        "Cannot configure framework_graph with a driver when not in deferred mode."));
  }

  SECTION("Ensure default driver executes one job cell")
  {
    g.provide(
       "provide_number",
       [](data_cell_index const&) -> unsigned int { return 42u; },
       concurrency::unlimited)
      .output_product("input", "number", "job");
    g.observe(
       "observe_number", [](unsigned int const) {}, concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .layer = "job", .suffix = "number"});

    g.execute();

    CHECK(g.execution_count("provide_number") == 1);
    CHECK(g.execution_count("observe_number") == 1);
  }
}

TEST_CASE("Throw on duplicate node registration", "[graph]")
{
  auto g = phlex::detail::framework_graph::with_default_driver();

  g.observe(
     "duplicate_name", [](unsigned int const) {}, concurrency::unlimited)
    .input_family(product_selector{.layer = "job"});
  g.observe(
     "duplicate_name", [](unsigned int const) {}, concurrency::unlimited)
    .input_family(product_selector{.layer = "job"});

  CHECK_THROWS_WITH(g.execute(),
                    Catch::Matchers::ContainsSubstring("Configuration errors") &&
                      Catch::Matchers::ContainsSubstring("duplicate_name"));
}

TEST_CASE("Allow late driver configuration", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("spill", {"job", 3});

  auto g = phlex::detail::framework_graph::without_driver();

  g.provide(
     "provide_number",
     [](data_cell_index const& index) -> unsigned int { return index.number(); },
     concurrency::unlimited)
    .output_product("input", "number", "spill");
  g.observe(
     "observe_number", [](unsigned int const /*number*/) {}, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "spill", .suffix = "number"});

  g.add_driver(gen);
  g.execute();

  CHECK(g.execution_count("provide_number") == 3);
  CHECK(g.execution_count("observe_number") == 3);
}

TEST_CASE("driver_proxy validates sources and generator", "[graph]")
{
  std::vector<phlex::source const*> sources{};
  auto src = std::make_unique<test_source>();
  sources.push_back(src.get());
  detail::driver_proxy proxy{sources};

  SECTION("Throw when source parameter count mismatches")
  {
    CHECK_THROWS_WITH(
      proxy.driver(fixed_hierarchy{}, [](data_cell_cursor) {}),
      Catch::Matchers::ContainsSubstring(
        "Number of source parameters of driver function does not match the number of sources ") &&
        Catch::Matchers::ContainsSubstring("specified in the configuration"));
  }

  SECTION("Throw when driver_builder is empty")
  {
    std::shared_ptr<test_driver_builder> const null_generator{nullptr};
    CHECK_THROWS_WITH(
      proxy.driver(null_generator),
      Catch::Matchers::ContainsSubstring("Cannot configure driver with an empty driver builder"));
  }
}

TEST_CASE("driver_proxy creates bundle from driver builder", "[graph]")
{
  detail::driver_proxy proxy{{}};
  auto const bundle = proxy.driver(std::make_shared<test_driver_builder>());

  CHECK(static_cast<bool>(bundle.driver));
}

TEST_CASE("Driver function receives registered source", "[graph]")
{
  auto g = phlex::detail::framework_graph::without_driver();
  g.add_source<test_source>("src");

  test_source const* received_src{nullptr};
  auto bundle = g.driver_proxy({"src"}).driver(
    fixed_hierarchy{},
    [&received_src](data_cell_cursor, test_source const& src) { received_src = &src; });
  g.add_driver(std::move(bundle));
  g.execute();

  CHECK(received_src != nullptr);
}

TEST_CASE("Driver function throws on source type mismatch", "[graph]")
{
  // Register other_source but declare test_source const& in the driver function.
  // The source downcast inside invoke_driver_with_sources throws with context.
  auto g = phlex::detail::framework_graph::without_driver();
  g.add_source<other_source>("src");

  auto bundle = g.driver_proxy({"src"}).driver(fixed_hierarchy{},
                                               [](data_cell_cursor, test_source const& /*src*/) {});
  g.add_driver(std::move(bundle));

  auto const expected_msg =
    fmt::format("Driver source type mismatch at source index 0: expected '{}' but got '{}'.",
                boost::core::demangle(typeid(test_source).name()),
                boost::core::demangle(typeid(other_source).name()));

  CHECK_THROWS_WITH(g.execute(), expected_msg);
}

TEST_CASE("Throw when configuring driver twice", "[graph]")
{
  auto g = phlex::detail::framework_graph::without_driver();

  auto gen = experimental::layer_generator::make();
  CHECK_NOTHROW(g.add_driver(gen));
  CHECK_THROWS_WITH(
    g.add_driver(gen),
    Catch::Matchers::ContainsSubstring("Driver has already been configured for framework_graph"));
}

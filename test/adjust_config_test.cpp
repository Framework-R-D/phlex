#include "phlex/app/load_module.hpp"
#include "phlex/app/run.hpp"
#include "phlex/core/framework_graph.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <utility>

using namespace phlex::detail::internal;

TEST_CASE("Adjust empty config", "[config]")
{
  auto const config = adjust_config("empty", {});
  CHECK(config.at("module_label").as_string() == "empty");
}

TEST_CASE("Adjust config with py only", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module";

  auto const config = adjust_config("", std::move(obj));
  CHECK(config.at("py").as_string() == "my_python_module");
  CHECK(config.at("cpp").as_string() == "pymodule");
}

TEST_CASE("Both py and cpp specified as strings", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module";
  obj["cpp"] = "my_other_python_phlex_module";

  auto const* const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed1
  - cpp: my_other_python_phlex_module
  - py: my_python_module)""";
  CHECK_THROWS_WITH(adjust_config("malformed1", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

TEST_CASE("Both py and cpp specified, py as string", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module";
  obj["cpp"] = 1;

  auto const* const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed2
  - py: my_python_module)""";
  CHECK_THROWS_WITH(adjust_config("malformed2", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

TEST_CASE("Both py and cpp specified, cpp as string", "[config]")
{
  boost::json::object obj;
  obj["py"] = 2;
  obj["cpp"] = "my_other_python_phlex_module";

  auto const* const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed3
  - cpp: my_other_python_phlex_module)""";
  CHECK_THROWS_WITH(adjust_config("malformed3", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

TEST_CASE("Loading resources requires a cpp parameter", "[config]")
{
  auto graph = phlex::detail::framework_graph::without_driver("test");

  CHECK_THROWS_WITH(
    phlex::detail::load_resource(graph, "my_resource", {}),
    Catch::Matchers::ContainsSubstring(
      "Missing 'cpp' parameter for my_resource -- only C++ resources are supported."));
}

TEST_CASE("A stage is required to run phlex", "[config]")
{
  CHECK_THROWS_WITH(phlex::detail::run({}, {}), "Must provide a 'stage' name.");
}

TEST_CASE("An empty stage cannot be used to run phlex", "[config]")
{
  phlex::detail::overridable_configuration const overrides{.stage = ""};
  CHECK_THROWS_WITH(phlex::detail::run({}, overrides), "Stage name cannot be empty.");
}

TEST_CASE("CURRENT is a reserved stage name for running phlex", "[config]")
{
  phlex::detail::overridable_configuration const overrides{.stage = "CURRENT"};
  CHECK_THROWS_WITH(phlex::detail::run({}, overrides), "'CURRENT' is a reserved stage name.");
}

TEST_CASE("Malformed driver configuration identifies its parameter", "[config]")
{
  boost::json::object configurations;
  configurations["driver"] = "not an object";

  phlex::detail::overridable_configuration const overrides{.stage = "test"};

  CHECK_THROWS_WITH(phlex::detail::run(configurations, overrides),
                    Catch::Matchers::ContainsSubstring("Error retrieving parameter 'driver' :"));
}

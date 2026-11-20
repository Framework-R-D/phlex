#include "phlex/configuration.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <oneapi/tbb/flow_graph.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST_CASE("algorithm_name tests", "[model]")
{
  using namespace phlex::detail;
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;

  SECTION("Create from string with colon")
  {
    auto an = algorithm_name::create("plugin:algo");
    CHECK(an.to_string() == "plugin:algo");
  }
  SECTION("Create from string without colon")
  {
    auto an = algorithm_name::create("algo");
    // For 'either' cases, the word is stored as algorithm_
    CHECK(an.to_string() == "algo");
  }
  SECTION("Create from char pointer")
  {
    auto an = algorithm_name::create("ptr:algo");
    CHECK(an.to_string() == "ptr:algo");
  }
  SECTION("Unqualified name has no plugin")
  {
    auto const an = algorithm_name::create("algo");
    CHECK_FALSE(an.plugin().has_value());
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Qualified name has plugin and algorithm")
  {
    auto const an = algorithm_name::create("plugin:algo");
    REQUIRE(an.plugin().has_value());
    CHECK(an.plugin().value() == "plugin"_id);
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Construct from typed components")
  {
    algorithm_name const unqualified{std::nullopt, "algo"_id};
    CHECK_FALSE(unqualified.plugin().has_value());
    CHECK(unqualified.to_string() == "algo");
    CHECK(unqualified == algorithm_name::create("algo"));

    algorithm_name const qualified{"plugin"_id, "algo"_id};
    REQUIRE(qualified.plugin().has_value());
    CHECK(qualified.to_string() == "plugin:algo");
    CHECK(qualified == algorithm_name::create("plugin:algo"));
    CHECK(qualified != unqualified);
  }
  SECTION("Create error - structural problems are reported as invalid_argument")
  {
    auto const *const forms = R"( Expected "algorithm" or "plugin_label:algorithm".)";
    auto const message = [forms](std::string_view text, std::string_view reason) {
      return std::string{"Invalid algorithm name \""} + std::string{text} +
             "\": " + std::string{reason} + forms;
    };

    CHECK_THROWS_AS(algorithm_name::create(""), std::invalid_argument);
    CHECK_THROWS_WITH(algorithm_name::create(""), message("", "it is empty."));
    CHECK_THROWS_WITH(algorithm_name::create("plugin:"),
                      message("plugin:", "the algorithm after ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name::create(":algo"),
                      message(":algo", "the plugin label before ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name::create(":"),
                      message(":", "the plugin label before ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name::create("a:b:c"),
                      message("a:b:c", "it contains more than one ':'."));
  }
  SECTION("Create error - malformed components are reported as invalid_argument")
  {
    auto const rule = std::string{phlex::detail::identifier_rule};

    CHECK_THROWS_AS(algorithm_name::create("_x"), std::invalid_argument);
    CHECK_THROWS_WITH(algorithm_name::create("_x"),
                      "Invalid algorithm name \"_x\": algorithm \"_x\" " + rule);
    CHECK_THROWS_WITH(algorithm_name::create("a[0]"),
                      "Invalid algorithm name \"a[0]\": algorithm \"a[0]\" " + rule);
    CHECK_THROWS_WITH(algorithm_name::create("a b"),
                      "Invalid algorithm name \"a b\": algorithm \"a b\" " + rule);
    CHECK_THROWS_WITH(algorithm_name::create("1p:a"),
                      "Invalid algorithm name \"1p:a\": plugin label \"1p\" " + rule);
    CHECK_THROWS_WITH(algorithm_name::create("pl ugin:algo"),
                      "Invalid algorithm name \"pl ugin:algo\": plugin label \"pl ugin\" " + rule);
    CHECK_THROWS_WITH(algorithm_name::create("p:1a"),
                      "Invalid algorithm name \"p:1a\": algorithm \"1a\" " + rule);
    // Only the first problem is reported, and the plugin label is checked first.
    CHECK_THROWS_WITH(algorithm_name::create("1p:1a"),
                      "Invalid algorithm name \"1p:1a\": plugin label \"1p\" " + rule);
  }
  SECTION("Create error - long text is shortened in the message")
  {
    std::string const text = std::string(100, 'a') + "-";
    std::string const shown = std::string(64, 'a') + "...";
    CHECK_THROWS_WITH(algorithm_name::create(text),
                      Catch::Matchers::ContainsSubstring("\"" + shown + "\"") &&
                        !Catch::Matchers::ContainsSubstring(text));
  }
  SECTION("Create from optional plugin and algorithm")
  {
    CHECK(algorithm_name::create(std::nullopt, "algo") == algorithm_name::create("algo"));
    CHECK(algorithm_name::create("plugin", "algo") == algorithm_name::create("plugin:algo"));
    // An empty plugin is not the same as an absent plugin.
    CHECK_THROWS_AS(algorithm_name::create("", "algo"), std::invalid_argument);
    CHECK_THROWS_AS(algorithm_name::create(std::nullopt, ""), std::invalid_argument);
    CHECK_THROWS_AS(algorithm_name::create("plugin", ""), std::invalid_argument);
    // The overload does not split on colons, so a colon in a component violates the character rule.
    CHECK_THROWS_AS(algorithm_name::create(std::nullopt, "a:b"), std::invalid_argument);
  }
  SECTION("Match both")
  {
    algorithm_name const an = algorithm_name::create("p:a");
    CHECK(an.match(algorithm_name::create("p:a")));
    CHECK_FALSE(an.match(algorithm_name::create("p:b")));
  }
  SECTION("Bulleted list of algorithm_name")
  {
    std::vector<algorithm_name> const ans = {algorithm_name::create("p:a1"),
                                             algorithm_name::create("p:a2")};
    CHECK(bulleted_list(ans) == "  - p:a1\n  - p:a2");
  }
  SECTION("Empty bulleted list")
  {
    CHECK(bulleted_list(std::vector<identifier>{}).empty());
    CHECK(bulleted_list(std::vector<algorithm_name>{}).empty());
  }
}

TEST_CASE("make_algorithm_name tests", "[core]")
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using internal::make_algorithm_name;

  SECTION("Without configuration the plugin is absent")
  {
    auto const name = make_algorithm_name(nullptr, "my_algorithm");
    CHECK_FALSE(name.plugin().has_value());
    CHECK(name.algorithm() == "my_algorithm"_id);
  }

  SECTION("With configuration the module label is the plugin")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    auto const name = make_algorithm_name(&config, "my_algorithm");
    REQUIRE(name.plugin().has_value());
    CHECK(name.plugin().value() == "my_module"_id);
    CHECK(name.to_string() == "my_module:my_algorithm");
  }

  SECTION("An empty algorithm name is rejected with the same message as create")
  {
    CHECK_THROWS_AS(make_algorithm_name(nullptr, ""), std::invalid_argument);
    CHECK_THROWS_WITH(
      make_algorithm_name(nullptr, ""),
      Catch::Matchers::ContainsSubstring("Invalid algorithm name \"\": it is empty"));

    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};
    CHECK_THROWS_WITH(
      make_algorithm_name(&config, ""),
      Catch::Matchers::ContainsSubstring(
        "Invalid algorithm name \"my_module:\": the algorithm after ':' is missing"));
  }

  SECTION("A malformed algorithm name is reported with the composed name")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    CHECK_THROWS_AS(make_algorithm_name(&config, "my-algorithm"), std::invalid_argument);
    CHECK_THROWS_WITH(
      make_algorithm_name(&config, "my-algorithm"),
      Catch::Matchers::ContainsSubstring("Invalid algorithm name "
                                         "\"my_module:my-algorithm\": algorithm "
                                         "\"my-algorithm\" " +
                                         std::string{phlex::detail::identifier_rule}));
    CHECK_THROWS_AS(make_algorithm_name(nullptr, "a[0]"), std::invalid_argument);
  }

  SECTION("A malformed module label is reported as a bad plugin label")
  {
    boost::json::object obj;
    obj["module_label"] = "1module";
    phlex::configuration const config{obj};

    CHECK_THROWS_WITH(make_algorithm_name(&config, "my_algorithm"),
                      Catch::Matchers::ContainsSubstring(
                        "plugin label \"1module\" " + std::string{phlex::detail::identifier_rule}));
  }
}

TEST_CASE("add_to_error_messages tests", "[core]")
{
  using namespace phlex::detail::internal;

  std::vector<std::string> errors;
  add_to_error_messages(errors, "Node", "duplicate_node");

  REQUIRE(errors.size() == 1);
  CHECK(errors[0].contains("duplicate_node"));
}

TEST_CASE("Moving a registrar transfers node creation", "[core]")
{
  using namespace phlex::detail;

  tbb::flow::graph graph;
  declared_outputs outputs;
  std::vector<std::string> errors;
  int creations = 0;
  auto creator = [&](auto const&, auto const&) -> declared_output_ptr {
    ++creations;
    return std::make_unique<declared_output>(phlex::experimental::algorithm_name::create("output"),
                                             1,
                                             std::vector<std::string>{},
                                             graph,
                                             [](auto const&) {});
  };

  SECTION("Move construction")
  {
    {
      registrar<declared_output_ptr> original{outputs, errors};
      original.set_creator(creator);
      auto moved = std::move(original);
    }
    CHECK(creations == 1);
    CHECK(outputs.get("output") != nullptr);
  }

  SECTION("Move assignment")
  {
    {
      registrar<declared_output_ptr> original{outputs, errors};
      original.set_creator(creator);
      registrar<declared_output_ptr> moved{outputs, errors};
      moved = std::move(original);
    }
    CHECK(creations == 1);
    CHECK(outputs.get("output") != nullptr);
  }

  CHECK(errors.empty());
}

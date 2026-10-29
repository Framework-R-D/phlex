#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("algorithm_name tests", "[model]")
{
  using namespace phlex::detail;
  using namespace phlex::experimental;

  SECTION("Empty creator name is rejected")
  {
    CHECK_THROWS_AS(algorithm_name::create(""), std::runtime_error);
    CHECK_THROWS_AS(algorithm_name(""), std::runtime_error);
  }
  SECTION("Create from string with colon")
  {
    using namespace phlex::experimental::literals;
    auto an = algorithm_name::create("plugin:algo");
    CHECK(an.to_string() == "plugin:algo");
    CHECK(an.plugin().text() == "plugin");
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Create from string without colon has an explicitly empty plugin")
  {
    using namespace phlex::experimental::literals;
    auto an = algorithm_name::create("algo");
    // For 'either' cases, the word is stored as algorithm_
    CHECK(an.to_string() == "algo");
    CHECK(an.plugin().empty());
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Create rejects invalid components")
  {
    CHECK_THROWS_AS(algorithm_name::create(":find_hits"), std::runtime_error);
    CHECK_THROWS_AS(algorithm_name::create("reco:"), std::runtime_error);
    CHECK_THROWS_AS(algorithm_name::create(":"), std::runtime_error);
  }
  SECTION("Create from char pointer")
  {
    auto an = algorithm_name::create("ptr:algo");
    CHECK(an.to_string() == "ptr:algo");
  }
  SECTION("Create error - trailing colon")
  {
    CHECK_THROWS_AS(algorithm_name::create("plugin:"), std::runtime_error);
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

TEST_CASE("consumer tests", "[core]")
{
  using namespace phlex::experimental::literals;

  SECTION("Qualified creator has a present plugin")
  {
    auto an = phlex::experimental::algorithm_name::create("p:a");
    phlex::detail::consumer const c(an, {"pred1"});

    CHECK(c.name().to_string() == "p:a");
    CHECK_FALSE(c.plugin().empty());
    CHECK(c.plugin().text() == "p");
    CHECK(c.algorithm() == "a"_idq);
    CHECK(c.when().size() == 1);
  }

  SECTION("Unqualified creator has an explicitly empty plugin")
  {
    auto an = phlex::experimental::algorithm_name::create("a");
    phlex::detail::consumer const c(an, {});

    CHECK(c.name().to_string() == "a");
    CHECK(c.plugin().empty());
    CHECK(c.plugin() == phlex::experimental::identifier_or_empty{});
    CHECK(c.algorithm() == "a"_idq);
    CHECK(c.when().empty());
  }
}

TEST_CASE("make_algorithm_name tests", "[core]")
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using phlex::experimental::internal::make_algorithm_name;

  SECTION("A null configuration gives an absent plugin")
  {
    auto const name = make_algorithm_name(nullptr, "algo");
    CHECK(name.plugin().empty());
    CHECK(name.algorithm() == "algo"_id);
    CHECK(name.to_string() == "algo");
  }

  SECTION("A configured module label is the plugin")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    auto const name = make_algorithm_name(&config, "algo");
    CHECK_FALSE(name.plugin().empty());
    CHECK(name.plugin().text() == "my_module");
    CHECK(name.algorithm() == "algo"_id);
    CHECK(name.to_string() == "my_module:algo");
  }

  SECTION("A configuration without a module label is an error, not an absent plugin")
  {
    phlex::configuration const config{boost::json::object{}};
    CHECK_THROWS_AS(make_algorithm_name(&config, "algo"), std::runtime_error);
  }

  SECTION("A configured empty module label is an error, not an absent plugin")
  {
    boost::json::object obj;
    obj["module_label"] = "";
    phlex::configuration const config{obj};
    CHECK_THROWS_AS(make_algorithm_name(&config, "algo"), std::runtime_error);
  }
}

TEST_CASE("verify_name tests", "[core]")
{
  using namespace phlex::detail::internal;

  SECTION("non-empty name does nothing") { CHECK_NOTHROW(verify_name("valid_name", nullptr)); }

  SECTION("empty name throws") { CHECK_THROWS_AS(verify_name("", nullptr), std::runtime_error); }

  SECTION("empty name with config includes module label")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    try {
      verify_name("", &config);
      FAIL("Should have thrown");
    } catch (std::runtime_error const& e) {
      std::string const msg = e.what();
      CHECK(msg.contains("my_module"));
    }
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

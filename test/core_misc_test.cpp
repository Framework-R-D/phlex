#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include <catch2/catch_test_macros.hpp>

#include <boost/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("algorithm_name tests", "[model]")
{
  using namespace phlex::experimental;

  SECTION("Default constructor")
  {
    algorithm_name an;
    CHECK(an.full() == "");
  }
  SECTION("Create from string with colon")
  {
    auto an = algorithm_name::create("plugin:algo");
    CHECK(an.full() == "plugin:algo");
  }
  SECTION("Create from string without colon")
  {
    auto an = algorithm_name::create("algo");
    // For 'either' cases, the first word is stored as plugin_
    CHECK(an.full() == "algo:");
  }
  SECTION("Create from char pointer")
  {
    auto an = algorithm_name::create("ptr:algo");
    CHECK(an.full() == "ptr:algo");
  }
  SECTION("Create error - trailing colon")
  {
    CHECK_THROWS_AS(algorithm_name::create("plugin:"), std::runtime_error);
  }
  SECTION("Match neither")
  {
    algorithm_name an = algorithm_name::create("p:a");
    algorithm_name other; // neither
    CHECK(an.match(other));
  }
  SECTION("Match either")
  {
    algorithm_name an = algorithm_name::create("p:a");
    CHECK(an.match(algorithm_name::create("p")));
    CHECK(an.match(algorithm_name::create("a")));
  }
  SECTION("Match both")
  {
    algorithm_name an = algorithm_name::create("p:a");
    CHECK(an.match(algorithm_name::create("p:a")));
    CHECK_FALSE(an.match(algorithm_name::create("p:b")));
  }
}

TEST_CASE("consumer tests", "[core]")
{
  using namespace phlex::experimental;
  algorithm_name an = algorithm_name::create("p:a");
  consumer c(an, {"pred1"});

  CHECK(c.full_name() == "p:a");
  CHECK(c.plugin() == "p");
  CHECK(c.algorithm() == "a");
  CHECK(c.when().size() == 1);
}

TEST_CASE("verify_name tests", "[core]")
{
  using namespace phlex::experimental::detail;

  SECTION("non-empty name does nothing") { CHECK_NOTHROW(verify_name("valid_name", nullptr)); }

  SECTION("empty name throws") { CHECK_THROWS_AS(verify_name("", nullptr), std::runtime_error); }

  SECTION("empty name with config includes module label")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration config{obj};

    try {
      verify_name("", &config);
      FAIL("Should have thrown");
    } catch (std::runtime_error const& e) {
      std::string msg = e.what();
      CHECK(msg.find("my_module") != std::string::npos);
    }
  }
}

TEST_CASE("add_to_error_messages tests", "[core]")
{
  using namespace phlex::experimental::detail;

  std::vector<std::string> errors;
  add_to_error_messages(errors, "duplicate_node");

  REQUIRE(errors.size() == 1);
  CHECK(errors[0].find("duplicate_node") != std::string::npos);
}

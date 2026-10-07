#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("algorithm_name tests", "[model]")
{
  using namespace phlex::detail;
  using namespace phlex::experimental;

  SECTION("Default constructor")
  {
    algorithm_name const an;
    CHECK(an.to_string().empty());
  }
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
  auto an = phlex::experimental::algorithm_name::create("p:a");
  phlex::detail::consumer const c(an, {"pred1"});

  CHECK(c.name().to_string() == "p:a");
  CHECK(c.plugin() == "p"_idq);
  CHECK(c.algorithm() == "a"_idq);
  CHECK(c.when().size() == 1);
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

TEST_CASE("A throwing registrar creator is not retried by the destructor", "[core]")
{
  using namespace phlex::detail;

  declared_outputs outputs;
  std::vector<std::string> errors;
  int creations = 0;
  {
    registrar<declared_output_ptr> reg{outputs, errors};
    reg.set_creator([&](auto const&, auto const&) -> declared_output_ptr {
      ++creations;
      throw std::runtime_error{"creator failed"};
    });
    CHECK_THROWS_WITH(reg.set_output_product_suffixes({}), "creator failed");
  }
  CHECK(creations == 1);
  CHECK(outputs.get("output") == nullptr);
  CHECK(errors.empty());
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

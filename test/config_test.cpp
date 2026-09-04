#include "phlex/configuration.hpp"

#include "boost/json.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex;
using namespace Catch::Matchers;

TEST_CASE("Check parameter-retrieval errors", "[config]")
{
  boost::json::object underlying_config;
  underlying_config["b"] = 2.5;
  configuration const config{underlying_config};

  CHECK(config.keys() == std::vector<std::string>{"b"});

  CHECK_THROWS_WITH(config.get<int>("a"), ContainsSubstring("Error retrieving parameter 'a'"));
  CHECK_THROWS_WITH(config.get<std::string>("b"),
                    ContainsSubstring("Error retrieving parameter 'b'"));
}

TEST_CASE("Retrieve value that is a configuration object", "[config]")
{
  boost::json::object underlying_config;
  underlying_config["nested_table"] = boost::json::object{};
  configuration const config{underlying_config};
  auto nested_table = config.get<configuration>("nested_table");
  CHECK(nested_table.keys().empty());
}

TEST_CASE("Retrieve product_selector", "[config]")
{
  boost::json::object input;
  input["creator"] = "tracks_alg";
  input["suffix"] = "tracks";
  input["layer"] = "job";

  boost::json::object unconstrained_input;

  boost::json::object malformed_input1;
  malformed_input1["creator"] = "test_alg";
  malformed_input1["suffix"] = 16.; // Should be string
  malformed_input1["layer"] = "job";

  boost::json::object malformed_input2;
  malformed_input2["creator"] = "hits";
  malformed_input2["layer"] = "";

  boost::json::object malformed_input3;
  malformed_input3["creator"] = "";
  malformed_input3["layer"] = "job";

  boost::json::object underlying_config;
  underlying_config["input"] = std::move(input);
  underlying_config["unconstrained"] = std::move(unconstrained_input);
  underlying_config["malformed1"] = std::move(malformed_input1);
  underlying_config["malformed2"] = std::move(malformed_input2);
  underlying_config["malformed3"] = std::move(malformed_input3);
  configuration config{underlying_config};

  auto input_query = config.get<product_selector>("input");
  CHECK(input_query.match(
    product_selector{.creator = "tracks_alg", .layer = "job", .suffix = "tracks"}));
  auto unconstrained_query = config.get<product_selector>("unconstrained");
  CHECK_FALSE(unconstrained_query.creator);
  CHECK_FALSE(unconstrained_query.layer);
  CHECK_THROWS_WITH(config.get<product_selector>("malformed1"),
                    ContainsSubstring("Error retrieving parameter 'malformed1'") &&
                      ContainsSubstring("not a string"));
  CHECK_THROWS_WITH(config.get<product_selector>("malformed2"),
                    ContainsSubstring("Error retrieving parameter 'malformed2'") &&
                      ContainsSubstring("Cannot specify the empty string as a data layer."));
  CHECK_THROWS_WITH(config.get<product_selector>("malformed3"),
                    ContainsSubstring("Error retrieving parameter 'malformed3'") &&
                      ContainsSubstring("Cannot specify product with empty creator name."));
}

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

TEST_CASE("Retrieve product_query", "[config]")
{
  boost::json::object input;
  input["creator"] = "tracks_alg";
  input["suffix"] = "tracks";
  input["layer"] = "job";

  boost::json::object malformed_input1;
  malformed_input1["creator"] = "test_alg";
  malformed_input1["suffix"] = 16.; // Should be string
  malformed_input1["layer"] = "job";

  boost::json::object malformed_input2;
  malformed_input2["creator"] = "hits";
  malformed_input2["level"] = "should be layer, not level";

  boost::json::object underlying_config;
  underlying_config["input"] = std::move(input);
  underlying_config["malformed1"] = std::move(malformed_input1);
  underlying_config["malformed2"] = std::move(malformed_input2);
  configuration config{underlying_config};

  auto input_query = config.get<product_query>("input");
  CHECK(
    input_query.match(product_query{.creator = "tracks_alg", .layer = "job", .suffix = "tracks"}));
  CHECK_THROWS_WITH(config.get<product_query>("malformed1"),
                    ContainsSubstring("Error retrieving parameter 'malformed1'") &&
                      ContainsSubstring("not a string"));
  CHECK_THROWS_WITH(config.get<product_query>("malformed2"),
                    ContainsSubstring("Error retrieving parameter 'malformed2'") &&
                      ContainsSubstring("Error retrieving parameter 'layer'"));
}

#include "phlex/configuration.hpp"

#include "boost/json.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex::experimental;

TEST_CASE("Check parameter-retrieval errors", "[config]")
{
  boost::json::object underlying_config;
  underlying_config["b"] = 2.5;
  configuration const config{underlying_config};

  CHECK(config.keys() == std::vector<std::string>{"b"});

  CHECK_THROWS_WITH(config.get<int>("a"),
                    Catch::Matchers::ContainsSubstring("Error retrieving parameter 'a'"));
  CHECK_THROWS_WITH(config.get<std::string>("b"),
                    Catch::Matchers::ContainsSubstring("Error retrieving parameter 'b'"));
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
  input["product"] = "tracks";
  input["layer"] = "job";

  boost::json::object underlying_config;
  underlying_config["input"] = std::move(input);
  configuration config{underlying_config};

  auto input_query = config.get<product_query>("input");
  CHECK(input_query == "tracks"_in("job"));
}

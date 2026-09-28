#include "phlex/app/run.hpp"
#include "test/ostream_logger.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>

// Regression test for issue #918.
TEST_CASE("A plugin cannot replace Phlex's default logger", "[plugins]")
{
  // The resource plugin replaces the default logger with one that discards messages.
  // The driver lets the workflow complete without needing other plugins.
  boost::json::object const configurations{
    {"resources", {{"replace_default_logger", {{"cpp", "replace_default_logger"}}}}},
    {"driver", {{"cpp", "generate_layers"}}}};

  // run() restores this capturing logger after plugin setup. The graph logs resource
  // usage during destruction, before run() returns.
  std::ostringstream output;
  auto logger = phlex::test::use_ostream_logger(output);
  REQUIRE_NOTHROW(phlex::detail::run(configurations, {.stage = "test"}));
  CHECK_THAT(output.str(), Catch::Matchers::ContainsSubstring("Max. RSS:"));
}

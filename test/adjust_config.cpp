#include "phlex/app/load_module.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "boost/json.hpp"

using namespace phlex::experimental::detail;

TEST_CASE("Adjust empty config", "[config]")
{
  auto const config = adjust_config("empty", {});
  CHECK(config.at("module_label").as_string() == "empty");
}

TEST_CASE("Adjust config with py only", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module.py";

  auto const config = adjust_config("", std::move(obj));
  CHECK(config.at("py").as_string() == "my_python_module.py");
  CHECK(config.at("cpp").as_string() == "pymodule");
}

TEST_CASE("Both py and cpp specified as strings", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module.py";
  obj["cpp"] = "my_other_python_phlex_module";

  auto const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed1
  - cpp: my_other_python_phlex_module
  - py: my_python_module.py)""";
  CHECK_THROWS_WITH(adjust_config("malformed1", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

TEST_CASE("Both py and cpp specified, py as string", "[config]")
{
  boost::json::object obj;
  obj["py"] = "my_python_module.py";
  obj["cpp"] = 1;

  auto const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed2
  - py: my_python_module.py)""";
  CHECK_THROWS_WITH(adjust_config("malformed2", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

TEST_CASE("Both py and cpp specified, cpp as string", "[config]")
{
  boost::json::object obj;
  obj["py"] = 2;
  obj["cpp"] = "my_other_python_phlex_module";

  auto const err_msg = R"""(Both 'cpp' and 'py' parameters specified for malformed3
  - cpp: my_other_python_phlex_module)""";
  CHECK_THROWS_WITH(adjust_config("malformed3", std::move(obj)),
                    Catch::Matchers::ContainsSubstring(err_msg));
}

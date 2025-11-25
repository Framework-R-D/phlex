#include "phlex/model/product_matcher.hpp"

#include "catch2/catch_test_macros.hpp"


using namespace phlex::experimental;

namespace {

  // Spec: [level path spec/][[module name][@node name]:]product name

  // std::string const optional_level_path{R"((?:(.*)/)?)"};
  // std::string const optional_qualified_node_name{R"((?:(\w+)?(?:@(\w+))?:)?)"};
  // std::string const product_name{R"((\w+))"};
  // std::regex const pattern{optional_level_path + optional_qualified_node_name + product_name};
  // void print(std::string const& spec)
  // {
  //   std::smatch submatches;
  //   CHECK(std::regex_match(spec, submatches, pattern));
  //   REQUIRE(submatches.size() == 5);
  //   for (std::size_t i = 0; i != submatches.size(); ++i) {
  //     std::cout << submatches[i] << '\n';
  //   }
  // }
}

// TEST_CASE("Regex", "[data model]")
// {
//   print("a/b/c/loaded_module@node:product_name");
//   print("help");
// }

TEST_CASE("Simple product matcher", "[data model]")
{
  auto const spec = "event/loaded_module@add:sum";
  product_matcher matcher{spec};
  CHECK(matcher.level_path() == "event");
  CHECK(matcher.module_name() == "loaded_module");
  CHECK(matcher.node_name() == "add");
  CHECK(matcher.product_name() == "sum");
  CHECK(matcher.encode() == spec);
}

TEST_CASE("Product matcher with default level path", "[data model]")
{
  product_matcher matcher{"loaded_module@add:sum"};
  CHECK(matcher.level_path() == "*");
  CHECK(matcher.module_name() == "loaded_module");
  CHECK(matcher.node_name() == "add");
  CHECK(matcher.product_name() == "sum");
  CHECK(matcher.encode() == "*/loaded_module@add:sum");

  CHECK(product_matcher{"*/loaded_module@add:sum"}.encode() == "*/loaded_module@add:sum");
}

TEST_CASE("Product matcher with multi-part level path", "[data model]")
{
  product_matcher matcher{"a/b/c/loaded_module@add:sum"};
  CHECK(matcher.level_path() == "a/b/c");
  CHECK(matcher.module_name() == "loaded_module");
  CHECK(matcher.node_name() == "add");
  CHECK(matcher.product_name() == "sum");
  CHECK(matcher.encode() == "a/b/c/loaded_module@add:sum");
}

TEST_CASE("Product with only node spec", "[data model]")
{
  product_matcher matcher{"@add:sum"};
  CHECK(matcher.level_path() == "*");
  CHECK(matcher.module_name() == "");
  CHECK(matcher.node_name() == "add");
  CHECK(matcher.product_name() == "sum");
  CHECK(matcher.encode() == "*/@add:sum");
}

TEST_CASE("Product with only module spec", "[data model]")
{
  product_matcher matcher{"loaded_module:sum"};
  CHECK(matcher.level_path() == "*");
  CHECK(matcher.module_name() == "loaded_module");
  CHECK(matcher.node_name() == "");
  CHECK(matcher.product_name() == "sum");
  CHECK(matcher.encode() == "*/loaded_module@:sum");
}

// TEST_CASE("Ill-formed specs", "[data model]")
// {
//   CHECK_THROWS(
//     product_matcher{""},
//     Catch::Matchers::ContainsSubstring("Empty product matcher specifications are not allowed."));
//   CHECK_THROWS(
//     product_matcher{"a/b/c/add"},
//     Catch::Matchers::ContainsSubstring("The product matcher specification is missing a colon (:)."));
//   CHECK_THROWS(
//     product_matcher{"/a/b/c/add"},
//     Catch::Matchers::ContainsSubstring("The matcher specification may not start with a forward slash (/)."));
// }

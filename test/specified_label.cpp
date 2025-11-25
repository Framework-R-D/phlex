#include "phlex/core/specified_label.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;

TEST_CASE("Empty label", "[data model]")
{
  specified_label empty{};
  CHECK_THROWS(""_in);
  CHECK_THROWS(""_in(""));
}

TEST_CASE("Only name in label", "[data model]")
{
  specified_label label{.name="product"};
  CHECK(label == "product"_in);

  // Empty family string is interpreted as a wildcard--i.e. any family.
  CHECK(label == "product"_in(""));
}

TEST_CASE("Label with family", "[data model]")
{
  specified_label label{.name="product", .family={"event"}};
  CHECK(label == "product"_in("event"));
}

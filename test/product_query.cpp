#include "phlex/core/product_query.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;

TEST_CASE("Empty label", "[data model]")
{
  product_query empty{};
  CHECK_THROWS(""_in);
  CHECK_THROWS(""_in(""));
}

TEST_CASE("Only name in label", "[data model]")
{
  product_query label{"product"};
  CHECK(label == "product"_in);

  // Empty layer string is interpreted as a wildcard--i.e. any layer.
  CHECK(label == "product"_in(""));
}

TEST_CASE("Label with layer", "[data model]")
{
  product_query label{"product", {"event"}};
  CHECK(label == "product"_in("event"));
}

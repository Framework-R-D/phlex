#include "phlex/core/framework_graph.hpp"

#include "catch2/catch_test_macros.hpp"

#include <stdexcept>

using namespace phlex::experimental;

TEST_CASE("Catch STL exceptions", "[graph]")
{
  framework_graph g{[](framework_driver&) { throw std::runtime_error("STL error"); }};
  CHECK_THROWS_AS(g.execute(), std::exception);
}

TEST_CASE("Catch other exceptions", "[graph]")
{
  framework_graph g{[](framework_driver&) { throw 2.5; }};
  CHECK_THROWS_AS(g.execute(), double);
}

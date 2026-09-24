#include "test/form/data_products/unserializable.hpp"
#include "test/form/test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <sstream>

auto constexpr tech = form::technology::root_rntuple;

TEST_CASE("automatic streamer mode", "[form RNTuple]")
{
  unserializable empty(1);
  std::stringstream std_err_redirect;
  auto* original_cerr = std::cerr.rdbuf(std_err_redirect.rdbuf());

  form::test::write(tech, empty);

  std::cerr.rdbuf(original_cerr);

  REQUIRE(!std_err_redirect.str().empty());
}

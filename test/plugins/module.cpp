#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add).input_family("i", "j").output_products("sum");
  m.observe("verify", [](int actual) { assert(actual == 0); }).input_family("sum");
}

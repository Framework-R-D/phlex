#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex::experimental;

// TODO: Option to select which algorithm to run via configuration?

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family("i", "j")
    .output_products("sum");
  m.observe(
     "verify", [](int actual) -> void { assert(actual == 0); }, concurrency::unlimited)
    .input_family("sum");
}

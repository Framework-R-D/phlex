#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex::experimental;

// TODO: Option to select which algorithm to run via configuration?

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family("i"_in("event"), "j"_in("event"))
    .output_products("sum");
  m.observe(
     "verify", [](int actual) { assert(actual == 0); }, concurrency::unlimited)
    .input_family("sum"_in("event"));
}

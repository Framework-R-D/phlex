#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex::experimental;

// TODO: Option to select which algorithm to run via configuration?

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.with("add", test::add, concurrency::unlimited).transform("i", "j").to("sum");
  m.observe(
     "verify", [](int actual) -> void { assert(actual == 0); }, concurrency::unlimited)
    .input_family("sum");
}

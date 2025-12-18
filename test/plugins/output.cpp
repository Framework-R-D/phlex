#include "phlex/module.hpp"
#include "test/products_for_output.hpp"

using namespace phlex::experimental::test;

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.make<products_for_output>().output(
    "save", &products_for_output::save, phlex::experimental::concurrency::unlimited);
}

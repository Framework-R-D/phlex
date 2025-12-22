#include "phlex/module.hpp"
#include "test/products_for_output.hpp"

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m)
{
  using experimental::test::products_for_output;
  m.make<products_for_output>().output("save", &products_for_output::save, concurrency::unlimited);
}

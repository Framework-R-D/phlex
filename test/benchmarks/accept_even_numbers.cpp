#include "phlex/module.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex::experimental;
  m.predicate(
     "accept_even_numbers", [](int i) { return i % 2 == 0; }, concurrency::unlimited)
    .input_family(config.get<product_query>("consumes"));
}

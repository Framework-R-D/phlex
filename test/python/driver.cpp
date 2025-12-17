#include "phlex/module.hpp"

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.provide("provide_i", [](data_cell_index const& id) -> int { return id.number(); })
    .output_product(product_query{product_specification::create("i")});
  m.provide("provide_j", [](data_cell_index const& id) -> int { return -id.number() + 1; })
    .output_product(product_query{product_specification::create("i")});
}

#include "phlex/module.hpp"

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.provide("provide_i", [](data_cell_index const& id) -> int { return id.number(); })
    .output_product("i"_in("job"));
  m.provide("provide_j", [](data_cell_index const& id) -> int { return -id.number() + 1; })
    .output_product("j"_in("job"));
}

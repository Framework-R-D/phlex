#include "phlex/module.hpp"

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  using namespace phlex::experimental;
  m.provide("provide_id", [](data_cell_index const& id) { return id; })
    .output_product("id"_in("event"));
}

#include "phlex/source.hpp"

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) -> int { return id.number(); })
    .output_product("i"_in("event"));
  s.provide("provide_j", [](data_cell_index const& id) -> int { return -id.number(); })
    .output_product("j"_in("event"));
}

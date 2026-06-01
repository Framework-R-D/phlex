#include "phlex/source.hpp"

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) { return static_cast<int>(id.number()); })
    .output_product("input", "i", "event");
  s.provide("provide_j", [](data_cell_index const& id) { return -static_cast<int>(id.number()); })
    .output_product("input", "j", "event");
}

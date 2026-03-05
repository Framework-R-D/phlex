#include "phlex/source.hpp"

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) -> int { return id.number(); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "i"});
  s.provide("provide_j", [](data_cell_index const& id) -> int { return -id.number(); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "j"});
}

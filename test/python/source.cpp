#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) -> int { return id.number(); })
    .output_product(product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "i"_id});
  s.provide("provide_j", [](data_cell_index const& id) -> int { return -id.number() + 1; })
    .output_product(product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "j"_id});
}

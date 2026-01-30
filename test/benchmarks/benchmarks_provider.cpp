#include "phlex/source.hpp"

PHLEX_REGISTER_PROVIDERS(s)
{
  using namespace phlex;
  s.provide("provide_id", [](data_cell_index const& id) { return id; })
    .output_product(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "id"_id});
}

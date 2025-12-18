#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

#include <string>

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex::experimental;
  m.predicate(
     "accept_even_ids",
     [](data_cell_index const& id) { return id.number() % 2 == 0; },
     concurrency::unlimited)
    .input_family(config.get<product_query>("input"));
}

#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.predicate(
     "accept_even_ids",
     [](phlex::experimental::data_cell_index const& id) { return id.number() % 2 == 0; },
     phlex::experimental::concurrency::unlimited)
    .input_family(config.get<std::string>("product_name"));
}

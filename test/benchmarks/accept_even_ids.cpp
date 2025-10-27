#include "phlex/model/level_id.hpp"
#include "phlex/module.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.predicate(
     "accept_even_ids",
     [](phlex::experimental::level_id const& id) -> bool { return id.number() % 2 == 0; },
     phlex::experimental::concurrency::unlimited)
    .input_family(config.get<std::string>("product_name"));
}

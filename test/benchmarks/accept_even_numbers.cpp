#include "phlex/module.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.predicate(
     "accept_even_numbers",
     [](int i) { return i % 2 == 0; },
     phlex::experimental::concurrency::unlimited)
    .family(config.get<std::string>("consumes"));
}

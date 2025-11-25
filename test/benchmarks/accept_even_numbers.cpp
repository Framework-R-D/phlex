#include "phlex/module.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.predicate(
     "accept_even_numbers",
     [](int i) -> bool { return i % 2 == 0; },
     phlex::experimental::concurrency::unlimited)
    .input_family(config.get<std::string>("consumes"));
}

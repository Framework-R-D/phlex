#include "phlex/module.hpp"
#include "test/benchmarks/fibonacci_numbers.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.make<test::fibonacci_numbers>(config.get<int>("max_number"))
    .predicate(
      "accept", &test::fibonacci_numbers::accept, phlex::experimental::concurrency::unlimited)
    .family(config.get<std::string>("consumes"));
}

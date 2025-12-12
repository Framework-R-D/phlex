#include "phlex/module.hpp"
#include "test/benchmarks/fibonacci_numbers.hpp"

#include <string>

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex::experimental;
  m.make<test::fibonacci_numbers>(config.get<int>("max_number"))
    .predicate("accept", &test::fibonacci_numbers::accept, concurrency::unlimited)
    .input_family(config.get<product_query>("consumes"));
}

#include "phlex/module.hpp"
#include "test/benchmarks/fibonacci_numbers.hpp"

#include <cassert>

namespace test {
  class even_fibonacci_numbers {
  public:
    explicit even_fibonacci_numbers(int const n) : numbers_(n) {}
    void only_even(int const n) const { assert(n % 2 == 0 and numbers_.accept(n)); }

  private:
    fibonacci_numbers numbers_;
  };
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex;
  using namespace test;
  m.make<even_fibonacci_numbers>(config.get<int>("max_number"))
    .observe("only_even", &even_fibonacci_numbers::only_even, concurrency::unlimited)
    .input_family(config.get<product_query>("consumes"));
}

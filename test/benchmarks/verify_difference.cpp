#include "phlex/module.hpp"

#include <cassert>

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.observe(
     "verify_difference",
     [expected = config.get<int>("expected", 100)](int i, int j) -> void { assert(j - i == expected); },
     concurrency::unlimited)
    .input_family(config.get<std::string>("i", "b"), config.get<std::string>("j", "c"));
}

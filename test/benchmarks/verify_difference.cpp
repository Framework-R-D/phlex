#include "phlex/module.hpp"

#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.observe(
     "verify_difference",
     [expected = config.get<int>("expected", 100)](int i, int j) { assert(j - i == expected); },
     concurrency::unlimited)
    .input_family(product_query{config.get<std::string>("i", "b"), "event"},
                  product_query{config.get<std::string>("j", "c"), "event"});
}

#include "phlex/module.hpp"

#include <cassert>

using namespace phlex;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.observe(
     "verify_difference",
     [expected = config.get<int>("expected", 100)](int i, int j) { assert(j - i == expected); },
     concurrency::unlimited)
    .input_family(config.get<product_query>("i"), config.get<product_query>("j"));
}

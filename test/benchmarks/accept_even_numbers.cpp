#include "phlex/module.hpp"

#include <string>

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex;
  m.predicate(
     "accept_even_numbers", [](int i) { return i % 2 == 0; }, concurrency::unlimited)
    .input_family(config.get<product_query>("consumes"));
}

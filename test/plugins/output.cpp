#include "phlex/module.hpp"
#include "test/products_for_output.hpp"

using namespace phlex;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m)
{
  using experimental::test::products_for_output;
  m.make<products_for_output>().output("save", &products_for_output::save, concurrency::unlimited);
}

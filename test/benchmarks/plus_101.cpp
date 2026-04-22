#include "phlex/module.hpp"

using namespace phlex;

namespace {
  int plus_101(int i) noexcept { return i + 101; }
}

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.transform("plus_101", plus_101, concurrency::unlimited)
    .input_family(config.get<product_query>("input"))
    .output_product_suffixes("c");
}

#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

namespace {
  void read_index(int) {}
}

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex;
  m.observe("read_index", read_index, concurrency::unlimited)
    .input_family(config.get<product_query>("consumes"));
}

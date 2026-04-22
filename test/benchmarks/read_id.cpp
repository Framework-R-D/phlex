#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex;

namespace {
  void read_id(data_cell_index const&) {}
}

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m)
{
  m.observe("read_id", read_id, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "id"});
}

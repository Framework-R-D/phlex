#include "phlex/source.hpp"

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_PROVIDERS(s)
{
  using namespace phlex;
  s.provide("provide_id", [](data_cell_index const& id) { return id; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "id"});
}

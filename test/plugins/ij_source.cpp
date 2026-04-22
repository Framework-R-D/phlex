#include "phlex/source.hpp"

using namespace phlex;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) { return static_cast<int>(id.number()); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "i"});
  s.provide("provide_j", [](data_cell_index const& id) { return -static_cast<int>(id.number()); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "j"});
}

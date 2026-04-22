#include "phlex/source.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_PROVIDERS(s)
{
  using namespace phlex;
  s.provide(
     "provide_max_parallelism",
     [](data_cell_index const&) { return experimental::max_allowed_parallelism::active_value(); })
    .output_product(product_query{.creator = "input", .layer = "job", .suffix = "max_parallelism"});
}

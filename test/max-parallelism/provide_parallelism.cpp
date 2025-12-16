#include "phlex/source.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_PROVIDERS(s)
{
  s.provide("provide_max_parallelism",
            [](data_cell_index const&) { return max_allowed_parallelism::active_value(); })
    .output_product("max_parallelism"_in("job"));
}

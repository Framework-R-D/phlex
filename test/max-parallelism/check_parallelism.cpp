// =======================================================================================
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/module.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"

#include <cassert>

using namespace phlex::experimental;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.provide("provide_max_parallelism",
            [](data_cell_index const&) { return max_allowed_parallelism::active_value(); })
    .output_product("max_parallelism"_in("job"));

  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .input_family("max_parallelism"_in("job"));
}

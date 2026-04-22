// =======================================================================================
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/module.hpp"

#include <cassert>

using namespace phlex;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .input_family(product_query{.creator = "input", .layer = "job", .suffix = "max_parallelism"});
}

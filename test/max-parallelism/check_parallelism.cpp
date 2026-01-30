// =======================================================================================
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/module.hpp"

#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .input_family(
      product_query{.creator = "input"_id, .layer = "job"_id, .suffix = "max_parallelism"_id});
}

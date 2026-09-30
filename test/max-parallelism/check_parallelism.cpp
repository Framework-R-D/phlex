// =======================================================================================
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/core/product_selector.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/module.hpp"

#include <cassert>
#include <cstddef>
#include <string>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  auto const expected_stage = config.get<std::string>("expected_stage", "test");

  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .input_family(
      product_selector{.creator = "input", .layer = "job", .suffix = "max_parallelism"});

  m.transform("copy_parallelism", [](std::size_t const value) { return value; })
    .input_family(product_selector{.creator = "input", .layer = "job", .suffix = "max_parallelism"})
    .output_product_suffixes("stage_test");

  m.observe(
     "verify_stage",
     [expected_stage](handle<std::size_t> const value) { assert(value.stage() == expected_stage); })
    .input_family(product_selector{.creator = "verify", .layer = "job", .suffix = "stage_test"});
}

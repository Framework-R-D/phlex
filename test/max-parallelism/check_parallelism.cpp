// =======================================================================================
// This plugin contains *both* a source and a observe.  This is not normally what anyone
// would want to do.  But Boost's DLL support is robust enough to handle this
// circumstance.
//
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/model/product_store.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"

#include <cassert>

using namespace phlex::experimental;

namespace {
  class send_parallelism {
  public:
    void next(framework_driver& driver)
    {
      auto job_index = data_cell_index::base_ptr();
      driver.yield(job_index);
    }
  };
}

// Framework glue
PHLEX_EXPERIMENTAL_REGISTER_SOURCE(send_parallelism)
PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.provide("provide_max_parallelism",
            [](data_cell_index const&) {
              return max_allowed_parallelism::active_value();
            })
    .output_product("max_parallelism"_in("job"));

  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .input_family("max_parallelism"_in("job"));
}

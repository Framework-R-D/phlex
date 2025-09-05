// =======================================================================================
// This plugin contains *both* a source and a observe.  This is not normally what anyone
// would want to do.  But Boost's DLL support is robust enough to handle this
// circumstance.
//
// The goal is to test whether the maximum allowed parallelism (as specified by either the
// phlex command line, or configuration) agrees with what is expected.
// =======================================================================================

#include "phlex/concurrency.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <cassert>

using namespace phlex::experimental;

namespace {
  class send_parallelism {
  public:
    void next(framework_driver& driver)
    {
      auto job_store = product_store::base();
      job_store->add_product("max_parallelism",
                             concurrency::max_allowed_parallelism::active_value());
      driver.yield(job_store);
    }
  };
}

// Framework glue
PHLEX_EXPERIMENTAL_REGISTER_SOURCE(send_parallelism)
PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.observe("verify_expected",
            [expected = config.get<std::size_t>("expected_parallelism")](std::size_t actual) {
              assert(actual == expected);
            })
    .family("max_parallelism");
}

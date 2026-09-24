#include "phlex/module.hpp"
#include "test/plugins/resources_for_testing.hpp"

#include <cassert>

using namespace phlex;
using namespace test::plugins;

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.observe(
     "verify_resources",
     [](int const, configured_resource const* configured, serialized_resource const* serialized) {
       assert(configured != nullptr);
       assert(configured->value == 42);
       assert(serialized != nullptr);
     },
     concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "i"},
                  resource<configured_resource>{},
                  resource<serialized_resource>{});
}

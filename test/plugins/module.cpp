#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "i"},
                  product_query{.creator = "input", .layer = "event", .suffix = "j"})
    .output_product_suffixes("sum");
  m.observe(
     "verify", [](int actual) { assert(actual == 0); }, concurrency::unlimited)
    .input_family(product_query{.creator = "add", .layer = "event", .suffix = "sum"});
}

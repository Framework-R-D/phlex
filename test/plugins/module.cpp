#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family(product_query({.creator = "input"s, .layer = "event"s, .suffix = "i"s}),
                  product_query({.creator = "input"s, .layer = "event"s, .suffix = "j"s}))
    .output_products("sum");
  m.observe(
     "verify", [](int actual) { assert(actual == 0); }, concurrency::unlimited)
    .input_family(product_query({.creator = "add"s, .layer = "event"s, .suffix = "sum"s}));
}

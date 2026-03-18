#include "phlex/module.hpp"
#include "test/plugins/add.hpp"

#include <cassert>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "i"},
                  product_query{.creator = "input", .layer = "event", .suffix = "j"})
    .output_products("sum");
  m.observe(
     "verify", [](int actual) { assert(actual == 0); }, concurrency::unlimited)
    .input_family(product_query{.creator = "add", .layer = "event", .suffix = "sum"});
}

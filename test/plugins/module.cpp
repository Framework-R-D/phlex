#include "phlex/module.hpp"

#include "test/plugins/add.hpp"

#include <cassert>
#include <stdexcept>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.transform("add", test::add, concurrency::unlimited)
    .input_family(product_selector{.creator = "input", .layer = "event", .suffix = "i"},
                  product_selector{.creator = "input", .layer = "event", .suffix = "j"})
    .output_product_suffixes("sum");
  m.observe(
     "verify", [](int actual) { assert(actual == 0); }, concurrency::unlimited)
    .input_family(product_selector{.creator = "add", .layer = "event", .suffix = "sum"});
  m.observe(
     "verify_provider_stage",
     [](handle<int> const value) {
       if (value.stage() != "test") {
         throw std::runtime_error("Provider product has the wrong stage.");
       }
     },
     concurrency::unlimited)
    .input_family({.creator = "input", .layer = "event", .suffix = "i", .stage = "test"_id});
}

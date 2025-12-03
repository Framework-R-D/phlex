#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  int plus_101(int i) noexcept { return i + 101; }
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.transform("plus_101", plus_101, concurrency::unlimited)
    .input_family("a"_in("event"))
    .output_products("c");
}

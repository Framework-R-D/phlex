#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  auto plus_101(int i) noexcept -> int { return i + 101; }
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.transform("plus_101", plus_101, concurrency::unlimited).input_family("a").output_products("c");
}

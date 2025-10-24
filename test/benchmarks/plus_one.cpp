#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  auto plus_one(int i) noexcept -> int { return i + 1; }
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.with("plus_one", plus_one, concurrency::unlimited).transform("a").to("b");
}

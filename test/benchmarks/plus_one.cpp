#include "phlex/module.hpp"

using namespace phlex;

namespace {
  int plus_one(int i) noexcept { return i + 1; }
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.transform("plus_one", plus_one, concurrency::unlimited)
    .input_family(config.get<product_query>("input"))
    .output_products("b");
}

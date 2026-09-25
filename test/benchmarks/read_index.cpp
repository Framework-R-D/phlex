#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

namespace {
  void read_index(int) {}
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex;
  m.observe("read_index", read_index, concurrency::unlimited)
    .input_family(config.get<product_selector>("consumes"));
}

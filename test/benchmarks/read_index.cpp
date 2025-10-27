#include "phlex/module.hpp"

namespace {
  void read_index(int) {}
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.observe("read_index", read_index, phlex::experimental::concurrency::unlimited)
    .input_family(config.get<std::string>("consumes"));
}

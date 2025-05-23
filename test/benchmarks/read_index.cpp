#include "phlex/model/level_id.hpp"
#include "phlex/module.hpp"

namespace {
  void read_index(int) {}
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.with("read_index", read_index, phlex::experimental::concurrency::unlimited)
    .observe(config.get<std::string>("consumes"));
}

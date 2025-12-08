#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

namespace {
  void read_index(int) {}
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  using namespace phlex::experimental;
  m.observe("read_index", read_index, phlex::experimental::concurrency::unlimited)
    .input_family(product_query{config.get<std::string>("consumes"), "event"});
}

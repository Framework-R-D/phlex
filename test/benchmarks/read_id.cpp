#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

namespace {
  void read_id(phlex::experimental::data_cell_index const&) {}
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m)
{
  m.observe("read_id", read_id, phlex::experimental::concurrency::unlimited).input_family("id");
}

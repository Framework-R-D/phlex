#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  void read_id(data_cell_index const&) {}
}

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.observe("read_id", read_id, phlex::experimental::concurrency::unlimited)
    .input_family("id"_in("event"));
}

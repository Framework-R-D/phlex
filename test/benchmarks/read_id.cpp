#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex;

namespace {
  void read_id(data_cell_index const&) {}
}

PHLEX_REGISTER_ALGORITHMS(m)
{
  m.observe("read_id", read_id, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "id"});
}

#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  int last_index(data_cell_index const& id) { return static_cast<int>(id.number()); }
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.transform("last_index", last_index, concurrency::unlimited)
    .input_family("id"_in("event"))
    .output_products(config.get<std::string>("produces", "a"));
}

#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex;

namespace {
  int last_index(data_cell_index const& id) { return static_cast<int>(id.number()); }
}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.transform("last_index", last_index, concurrency::unlimited)
    .input_family(product_query{.creator = "input"_id, .layer = "event"_id, .suffix = "id"_id})
    .output_products(config.get<std::string>("produces", "a"));
}

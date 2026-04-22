#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"

using namespace phlex;

namespace {
  int last_index(data_cell_index const& id) { return static_cast<int>(id.number()); }
}

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  m.transform("last_index", last_index, concurrency::unlimited)
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "id"})
    .output_product_suffixes(config.get<std::string>("produces", "a"));
}

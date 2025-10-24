#include "phlex/model/level_id.hpp"
#include "phlex/module.hpp"

using namespace phlex::experimental;

namespace {
  auto last_index(level_id const& id) -> int { return static_cast<int>(id.number()); }
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  m.with("last_index", last_index, concurrency::unlimited)
    .transform("id")
    .to(config.get<std::string>("produces", "a"));
}

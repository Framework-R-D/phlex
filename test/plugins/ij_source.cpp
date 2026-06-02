#include "phlex/source.hpp"

using namespace phlex;

namespace {
  class signed_value_provider {
  public:
    int provide_i(data_cell_index const& id) const { return static_cast<int>(id.number()); }
  };
}

PHLEX_REGISTER_PROVIDERS(s)
{
  // Explicitly test s.make<T> syntax for provider registration, in addition to the lambda syntax.
  s.make<signed_value_provider>()
    .provide("provide_i", &signed_value_provider::provide_i)
    .output_product("input", "i", "event");
  s.provide("provide_j", [](data_cell_index const& id) { return -static_cast<int>(id.number()); })
    .output_product("input", "j", "event");
}

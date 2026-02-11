#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include <cstdint>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) -> int { return id.number() % 2; })
    .output_product("i"_in("job"));
  s.provide("provide_j",
            [](data_cell_index const& id) -> int { return 1 - (int)(id.number() % 2); })
    .output_product("j"_in("job"));
  s.provide("provide_k", [](data_cell_index const&) -> int { return 0; })
    .output_product("k"_in("job"));

  s.provide("provide_f1",
            [](data_cell_index const& id) -> float { return (float)((id.number() % 100) / 100.0); })
    .output_product("f1"_in("job"));
  s.provide(
     "provide_f2",
     [](data_cell_index const& id) -> float { return 1.0f - (float)((id.number() % 100) / 100.0); })
    .output_product("f2"_in("job"));

  s.provide(
     "provide_d1",
     [](data_cell_index const& id) -> double { return (double)((id.number() % 100) / 100.0); })
    .output_product("d1"_in("job"));
  s.provide("provide_d2",
            [](data_cell_index const& id) -> double {
              return 1.0 - (double)((id.number() % 100) / 100.0);
            })
    .output_product("d2"_in("job"));

  s.provide(
     "provide_u1",
     [](data_cell_index const& id) -> unsigned int { return (unsigned int)(id.number() % 2); })
    .output_product("u1"_in("job"));
  s.provide(
     "provide_u2",
     [](data_cell_index const& id) -> unsigned int { return 1 - (unsigned int)(id.number() % 2); })
    .output_product("u2"_in("job"));

  s.provide("provide_l1", [](data_cell_index const& id) -> long { return (long)(id.number() % 2); })
    .output_product("l1"_in("job"));
  s.provide("provide_l2",
            [](data_cell_index const& id) -> long { return 1 - (long)(id.number() % 2); })
    .output_product("l2"_in("job"));

  s.provide(
     "provide_ul1",
     [](data_cell_index const& id) -> unsigned long { return (unsigned long)(id.number() % 101); })
    .output_product("ul1"_in("job"));
  s.provide("provide_ul2",
            [](data_cell_index const& id) -> unsigned long {
              return 100 - (unsigned long)(id.number() % 101);
            })
    .output_product("ul2"_in("job"));

  s.provide("provide_b1", [](data_cell_index const& id) -> bool { return (id.number() % 2) == 0; })
    .output_product("b1"_in("job"));
  s.provide("provide_b2", [](data_cell_index const& id) -> bool { return (id.number() % 2) != 0; })
    .output_product("b2"_in("job"));
}

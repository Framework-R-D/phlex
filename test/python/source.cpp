#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include <cstdint>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i",
            [](data_cell_index const& id) { return static_cast<int>(id.number() % 2); })
    .output_product("input", "i", "event");
  s.provide("provide_j",
            [](data_cell_index const& id) -> int { return 1 - static_cast<int>(id.number() % 2); })
    .output_product("input", "j", "event");
  s.provide("provide_k", [](data_cell_index const&) -> int { return 0; })
    .output_product("input", "k", "event");

  s.provide("provide_f1",
            [](data_cell_index const& id) -> float {
              return static_cast<float>(id.number() % 100u) / 100.0f;
            })
    .output_product("input", "f1", "event");
  s.provide("provide_f2",
            [](data_cell_index const& id) -> float {
              return 1.0f - static_cast<float>(id.number() % 100u) / 100.0f;
            })
    .output_product("input", "f2", "event");

  s.provide("provide_d1",
            [](data_cell_index const& id) -> double {
              return static_cast<double>(id.number() % 100u) / 100.0;
            })
    .output_product("input", "d1", "event");
  s.provide("provide_d2",
            [](data_cell_index const& id) -> double {
              return 1.0 - static_cast<double>(id.number() % 100u) / 100.0;
            })
    .output_product("input", "d2", "event");

  s.provide("provide_u1",
            [](data_cell_index const& id) -> unsigned int {
              return static_cast<unsigned int>(id.number() % 2);
            })
    .output_product("input", "u1", "event");
  s.provide("provide_u2",
            [](data_cell_index const& id) -> unsigned int {
              return 1u - static_cast<unsigned int>(id.number() % 2);
            })
    .output_product("input", "u2", "event");

  s.provide("provide_l1",
            [](data_cell_index const& id) -> long { return static_cast<long>(id.number() % 2); })
    .output_product("input", "l1", "event");
  s.provide(
     "provide_l2",
     [](data_cell_index const& id) -> long { return 1L - static_cast<long>(id.number() % 2); })
    .output_product("input", "l2", "event");

  s.provide("provide_ul1",
            [](data_cell_index const& id) -> unsigned long {
              return static_cast<unsigned long>(id.number() % 101);
            })
    .output_product("input", "ul1", "event");
  s.provide("provide_ul2",
            [](data_cell_index const& id) -> unsigned long {
              return 100ul - static_cast<unsigned long>(id.number() % 101);
            })
    .output_product("input", "ul2", "event");

  s.provide("provide_b1", [](data_cell_index const& id) -> bool { return (id.number() % 2) == 0; })
    .output_product("input", "b1", "event");
  s.provide("provide_b2", [](data_cell_index const& id) -> bool { return (id.number() % 2) != 0; })
    .output_product("input", "b2", "event");
}

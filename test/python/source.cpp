#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include <cstdint>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i",
            [](data_cell_index const& id) { return static_cast<int>(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "i"});
  s.provide("provide_j",
            [](data_cell_index const& id) -> int { return 1 - static_cast<int>(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "j"});
  s.provide("provide_k", [](data_cell_index const&) -> int { return 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "k"});

  s.provide(
     "provide_f1",
     [](data_cell_index const& id) -> float {
       return static_cast<float>(id.number() % 100u) / 100.0f;
     })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "f1"});
  s.provide("provide_f2",
            [](data_cell_index const& id) -> float {
              return 1.0f - static_cast<float>(id.number() % 100u) / 100.0f;
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "f2"});

  s.provide(
     "provide_d1",
     [](data_cell_index const& id) -> double {
       return static_cast<double>(id.number() % 100u) / 100.0;
     })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "d1"});
  s.provide("provide_d2",
            [](data_cell_index const& id) -> double {
              return 1.0 - static_cast<double>(id.number() % 100u) / 100.0;
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "d2"});

  s.provide(
     "provide_u1",
     [](data_cell_index const& id) -> unsigned int {
       return static_cast<unsigned int>(id.number() % 2);
     })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "u1"});
  s.provide(
     "provide_u2",
     [](data_cell_index const& id) -> unsigned int {
       return 1u - static_cast<unsigned int>(id.number() % 2);
     })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "u2"});

  s.provide("provide_l1", [](data_cell_index const& id) -> long {
    return static_cast<long>(id.number() % 2);
  })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "l1"});
  s.provide("provide_l2",
            [](data_cell_index const& id) -> long {
              return 1L - static_cast<long>(id.number() % 2);
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "l2"});

  s.provide(
     "provide_ul1",
     [](data_cell_index const& id) -> unsigned long {
       return static_cast<unsigned long>(id.number() % 101);
     })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "ul1"});
  s.provide("provide_ul2",
            [](data_cell_index const& id) -> unsigned long {
              return 100ul - static_cast<unsigned long>(id.number() % 101);
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "ul2"});

  s.provide("provide_b1", [](data_cell_index const& id) -> bool { return (id.number() % 2) == 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "b1"});
  s.provide("provide_b2", [](data_cell_index const& id) -> bool { return (id.number() % 2) != 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "b2"});
}

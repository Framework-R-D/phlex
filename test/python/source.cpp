#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include <cstdint>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(s)
{
  s.provide("provide_i", [](data_cell_index const& id) -> int { return id.number() % 2; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "i"});
  s.provide("provide_j",
            [](data_cell_index const& id) -> int { return 1 - (int)(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "j"});
  s.provide("provide_k", [](data_cell_index const&) -> int { return 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "k"});

  s.provide("provide_f1",
            [](data_cell_index const& id) -> float { return (float)((id.number() % 100) / 100.0); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "f1"});
  s.provide(
     "provide_f2",
     [](data_cell_index const& id) -> float { return 1.0f - (float)((id.number() % 100) / 100.0); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "f2"});

  s.provide(
     "provide_d1",
     [](data_cell_index const& id) -> double { return (double)((id.number() % 100) / 100.0); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "d1"});
  s.provide("provide_d2",
            [](data_cell_index const& id) -> double {
              return 1.0 - (double)((id.number() % 100) / 100.0);
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "d2"});

  s.provide(
     "provide_u1",
     [](data_cell_index const& id) -> unsigned int { return (unsigned int)(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "u1"});
  s.provide(
     "provide_u2",
     [](data_cell_index const& id) -> unsigned int { return 1 - (unsigned int)(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "u2"});

  s.provide("provide_l1", [](data_cell_index const& id) -> long { return (long)(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "l1"});
  s.provide("provide_l2",
            [](data_cell_index const& id) -> long { return 1 - (long)(id.number() % 2); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "l2"});

  s.provide(
     "provide_ul1",
     [](data_cell_index const& id) -> unsigned long { return (unsigned long)(id.number() % 101); })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "ul1"});
  s.provide("provide_ul2",
            [](data_cell_index const& id) -> unsigned long {
              return 100 - (unsigned long)(id.number() % 101);
            })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "ul2"});

  s.provide("provide_b1", [](data_cell_index const& id) -> bool { return (id.number() % 2) == 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "b1"});
  s.provide("provide_b2", [](data_cell_index const& id) -> bool { return (id.number() % 2) != 0; })
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "b2"});
}

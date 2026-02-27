#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <iostream>

using namespace phlex;
using namespace oneapi::tbb;

namespace {
  data_cell_index provide_index(data_cell_index const& index) { return index; }

  void check_two_ids(data_cell_index const& parent_id, data_cell_index const& id)
  {
    CHECK(parent_id.depth() + 1ull == id.depth());
    CHECK(parent_id.hash() == id.parent()->hash());
  }

  void check_three_ids(data_cell_index const& grandparent_id,
                       data_cell_index const& parent_id,
                       data_cell_index const& id)
  {
    CHECK(id.depth() == 3ull);
    CHECK(parent_id.depth() == 2ull);
    CHECK(grandparent_id.depth() == 1ull);

    CHECK(grandparent_id.hash() == parent_id.parent()->hash());
    CHECK(parent_id.hash() == id.parent()->hash());
    CHECK(grandparent_id.hash() == id.parent()->parent()->hash());
  }
}

TEST_CASE("Testing families", "[data model]")
{
  experimental::layer_generator gen;
  gen.add_layer("run", {"job", 1});
  gen.add_layer("subrun", {"run", 1});
  gen.add_layer("event", {"subrun", 1});

  experimental::framework_graph g{driver_for_test(gen), 2};

  // Wire up providers for each level
  g.provide("run_id_provider", provide_index, concurrency::unlimited)
    .output_product(product_query{.creator = "dummy"_id, .layer = "run"_id, .suffix = "id"_id});
  g.provide("subrun_id_provider", provide_index, concurrency::unlimited)
    .output_product(product_query{.creator = "dummy"_id, .layer = "subrun"_id, .suffix = "id"_id});
  g.provide("event_id_provider", provide_index, concurrency::unlimited)
    .output_product(product_query{.creator = "dummy"_id, .layer = "event"_id, .suffix = "id"_id});

  g.observe("se", check_two_ids)
    .input_family(product_query{.creator = "dummy"_id, .layer = "subrun"_id, .suffix = "id"_id},
                  product_query{.creator = "dummy"_id, .layer = "event"_id, .suffix = "id"_id});
  g.observe("rs", check_two_ids)
    .input_family(product_query{.creator = "dummy"_id, .layer = "run"_id, .suffix = "id"_id},
                  product_query{.creator = "dummy"_id, .layer = "subrun"_id, .suffix = "id"_id});
  g.observe("rse", check_three_ids)
    .input_family(product_query{.creator = "dummy"_id, .layer = "run"_id, .suffix = "id"_id},
                  product_query{.creator = "dummy"_id, .layer = "subrun"_id, .suffix = "id"_id},
                  product_query{.creator = "dummy"_id, .layer = "event"_id, .suffix = "id"_id});
  g.execute();

  CHECK(g.execution_count("se") == 1ull);
  CHECK(g.execution_count("rs") == 1ull);
  CHECK(g.execution_count("rse") == 1ull);

  CHECK(g.execution_count("run_id_provider") == 1ull);
  CHECK(g.execution_count("subrun_id_provider") == 1ull);
  CHECK(g.execution_count("event_id_provider") == 1ull);
}

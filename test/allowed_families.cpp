#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_id.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <iostream>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace {

  void cells_to_process(framework_driver& driver)
  {
    auto job_store = product_store::base();
    job_store->add_product("id", *job_store->id());
    driver.yield(job_store);

    auto run_store = job_store->make_child(0, "run");
    run_store->add_product("id", *run_store->id());
    driver.yield(run_store);

    auto subrun_store = run_store->make_child(0, "subrun");
    subrun_store->add_product("id", *subrun_store->id());
    driver.yield(subrun_store);

    auto event_store = subrun_store->make_child(0, "event");
    event_store->add_product("id", *event_store->id());
    driver.yield(event_store);
  }

  void check_two_ids(data_cell_id const& parent_id, data_cell_id const& id)
  {
    CHECK(parent_id.depth() + 1ull == id.depth());
    CHECK(parent_id.hash() == id.parent()->hash());
  }

  void check_three_ids(data_cell_id const& grandparent_id,
                       data_cell_id const& parent_id,
                       data_cell_id const& id)
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
  framework_graph g{cells_to_process, 2};
  g.observe("se", check_two_ids).input_family("id"_in("subrun"), "id"_in("event"));
  g.observe("rs", check_two_ids).input_family("id"_in("run"), "id"_in("subrun"));
  g.observe("rse", check_three_ids)
    .input_family("id"_in("run"), "id"_in("subrun"), "id"_in("event"));
  g.execute();

  CHECK(g.execution_counts("se") == 1ull);
  CHECK(g.execution_counts("rs") == 1ull);
  CHECK(g.execution_counts("rse") == 1ull);
}

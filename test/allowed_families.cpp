#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

#include <iostream>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace {

  void cells_to_process(framework_driver& driver)
  {
    auto job_index = data_cell_index::base_ptr();
    spdlog::info("Yielding job index with depth {}", job_index->depth());
    driver.yield(job_index);

    auto run_index = job_index->make_child(0, "run");
    spdlog::info("Yielding run index with depth {}", run_index->depth());
    driver.yield(run_index);

    auto subrun_index = run_index->make_child(0, "subrun");
    spdlog::info("Yielding subrun index with depth {}", subrun_index->depth());
    driver.yield(subrun_index);

    auto event_index = subrun_index->make_child(0, "event");
    spdlog::info("Yielding event index with depth {}", event_index->depth());
    driver.yield(event_index);
  }

  // Provider functions that return data_cell_index for each level
  data_cell_index provide_run_id(data_cell_index const& index) { 
    spdlog::info("Providing run id with depth {}", index.depth());
    return index; 
  }

  data_cell_index provide_subrun_id(data_cell_index const& index) { 
    spdlog::info("Providing subrun id with depth {}", index.depth()); 
    return index; 
  }

  data_cell_index provide_event_id(data_cell_index const& index) { 
    spdlog::info("Providing event id with depth {}", index.depth());
    return index; 
  }

  void check_two_ids(data_cell_index const& parent_id, data_cell_index const& id)
  {
    CHECK(parent_id.depth() + 1ull == id.depth());
    CHECK(parent_id.hash() == id.parent()->hash());
  }

/*************  ✨ Windsurf Command ⭐  *************/
/**
 * @brief Checks the relationships between three data_cell_index objects.
 *
 * @param grandparent_id The data_cell_index object at depth 1.
 * @param parent_id The data_cell_index object at depth 2, which is the parent of id.
 * @param id The data_cell_index object at depth 3.
 *
 * This function checks that the relationships between the three objects are correct.
 * Specifically, it checks that:
 *   - id is at depth 3
 *   - parent_id is at depth 2
 *   - grandparent_id is at depth 1
 *   - grandparent_id is the grandparent of id
 *   - parent_id is the parent of id
 */
/*******  f1db79b2-2a69-493d-ae03-20d77cee24b3  *******/
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
  framework_graph g{cells_to_process, 2};

  // Wire up providers for each level
  g.provide("run_id_provider", provide_run_id, concurrency::unlimited)
    .output_product("id"_in("run"));
  g.provide("subrun_id_provider", provide_subrun_id, concurrency::unlimited)
    .output_product("id"_in("subrun"));
  g.provide("event_id_provider", provide_event_id, concurrency::unlimited)
    .output_product("id"_in("event"));

  g.observe("se", check_two_ids).input_family("id"_in("subrun"), "id"_in("event"));
  g.observe("rs", check_two_ids).input_family("id"_in("run"), "id"_in("subrun"));
  g.observe("rse", check_three_ids)
    .input_family("id"_in("run"), "id"_in("subrun"), "id"_in("event"));
  g.execute();

  CHECK(g.execution_counts("se") == 1ull);
  CHECK(g.execution_counts("rs") == 1ull);
  CHECK(g.execution_counts("rse") == 1ull);
}

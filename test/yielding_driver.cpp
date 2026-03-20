#include "phlex/model/data_cell_index.hpp"
#include "phlex/utilities/async_driver.hpp"

#include "catch2/catch_test_macros.hpp"

#include "tbb/flow_graph.h"

#include <ranges>
#include <vector>

using namespace phlex;

void cells_to_process(experimental::async_driver<data_cell_index_ptr>& d)
{
  unsigned int const num_runs = 2;
  unsigned int const num_subruns = 2;
  unsigned int const num_spills = 3;

  auto job_index = d.yield(data_cell_index::job());
  for (unsigned int r : std::views::iota(0u, num_runs)) {
    auto run_index = d.yield(job_index->make_child("run", r));
    for (unsigned int sr : std::views::iota(0u, num_subruns)) {
      auto subrun_index = d.yield(run_index->make_child("subrun", sr));
      for (unsigned int spill : std::views::iota(0u, num_spills)) {
        d.yield(subrun_index->make_child("spill", spill));
      }
    }
  }
}

TEST_CASE("Async driver with TBB flow graph", "[async_driver]")
{
  experimental::async_driver<data_cell_index_ptr> drive{cells_to_process};
  std::vector<std::string> received_indices;

  tbb::flow::graph g{};
  tbb::flow::input_node source{g, [&drive](tbb::flow_control& fc) -> data_cell_index_ptr {
                                 if (auto next = drive()) {
                                   return *next;
                                 }
                                 fc.stop();
                                 return {};
                               }};
  tbb::flow::function_node receiver{
    g,
    tbb::flow::serial,
    [&received_indices](data_cell_index_ptr const& set_id) -> tbb::flow::continue_msg {
      received_indices.push_back(set_id->to_string());
      return {};
    }};

  make_edge(source, receiver);
  source.activate();
  g.wait_for_all();

  CHECK(received_indices.size() == 19);
}

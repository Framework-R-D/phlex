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

  auto job_id = data_cell_index::base_ptr();
  d.yield(job_id);
  for (unsigned int r : std::views::iota(0u, num_runs)) {
    auto run_id = job_id->make_child(r, "run");
    d.yield(run_id);
    for (unsigned int sr : std::views::iota(0u, num_subruns)) {
      auto subrun_id = run_id->make_child(sr, "subrun");
      d.yield(subrun_id);
      for (unsigned int spill : std::views::iota(0u, num_spills)) {
        d.yield(subrun_id->make_child(spill, "spill"));
      }
    }
  }
}

TEST_CASE("Async driver with TBB flow graph", "[async_driver]")
{
  experimental::async_driver<data_cell_index_ptr> drive{cells_to_process};
  std::vector<std::string> received_ids;

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
    [&received_ids](data_cell_index_ptr const& set_id) -> tbb::flow::continue_msg {
      received_ids.push_back(set_id->to_string());
      return {};
    }};

  make_edge(source, receiver);
  source.activate();
  g.wait_for_all();

  CHECK(received_ids.size() == 19);
}

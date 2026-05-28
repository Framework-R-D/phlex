#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/index_generator.hpp"
#include "phlex/utilities/resumable_driver.hpp"

#include "catch2/catch_test_macros.hpp"

#include "tbb/flow_graph.h"

#include <ranges>
#include <vector>

using namespace phlex;

namespace {
  index_generator make_indices(unsigned int num_runs,
                               unsigned int num_subruns,
                               unsigned int num_spills)
  {
    auto job_id = data_cell_index::job();
    co_yield job_id;
    for (unsigned int r : std::views::iota(0u, num_runs)) {
      auto run_id = job_id->make_child("run", r);
      co_yield run_id;
      for (unsigned int sr : std::views::iota(0u, num_subruns)) {
        auto subrun_id = run_id->make_child("subrun", sr);
        co_yield subrun_id;
        for (unsigned int spill : std::views::iota(0u, num_spills)) {
          co_yield subrun_id->make_child("spill", spill);
        }
      }
    }
  }

  void cells_to_process(experimental::resumable_driver<data_cell_index_ptr>& d)
  {
    for (auto const& index : make_indices(2, 2, 3)) {
      d.yield(index);
    }
  }
}

TEST_CASE("Resumable driver with TBB flow graph", "[resumable_driver]")
{
  experimental::resumable_driver<data_cell_index_ptr> drive{cells_to_process};
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

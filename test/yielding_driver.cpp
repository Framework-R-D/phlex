#include "phlex/model/data_cell_id.hpp"
#include "phlex/utilities/async_driver.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"
#include "tbb/flow_graph.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <ranges>
#include <vector>

using namespace phlex::experimental;

void cells_to_process(async_driver<data_cell_id_ptr>& d)
{
  unsigned int const num_runs = 2;
  unsigned int const num_subruns = 2;
  unsigned int const num_spills = 3;

  auto job_id = data_cell_id::base_ptr();
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

int main()
{
  async_driver<data_cell_id_ptr> drive{cells_to_process};
  tbb::flow::graph g{};
  tbb::flow::input_node source{g, [&drive](tbb::flow_control& fc) -> data_cell_id_ptr {
                                 if (auto next = drive()) {
                                   return *next;
                                 }
                                 fc.stop();
                                 return {};
                               }};
  tbb::flow::function_node receiver{
    g, tbb::flow::unlimited, [](data_cell_id_ptr const& set_id) -> tbb::flow::continue_msg {
      spdlog::info("Received {}", set_id->to_string());
      return {};
    }};

  make_edge(source, receiver);

  source.activate();
  g.wait_for_all();
}

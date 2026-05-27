#include "phlex/model/index_generator.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "catch2/catch_test_macros.hpp"

#include <vector>

using namespace phlex;

namespace {
  index_generator make_indices()
  {
    auto job = data_cell_index::job();
    co_yield job;

    auto run = job->make_child("run", 2);
    co_yield run;

    co_yield run->make_child("subrun", 7);
  }
}

TEST_CASE("index_generator yields data_cell_index_ptr values", "[data model]")
{
  std::vector<std::string> seen;
  for (auto const& index : make_indices()) {
    seen.push_back(index->to_string());
  }

  CHECK(seen == std::vector<std::string>{"[]", "[run:2]", "[run:2, subrun:7]"});
}

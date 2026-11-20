#ifndef TEST_DATA_CELL_INDEX_HELPERS_HPP
#define TEST_DATA_CELL_INDEX_HELPERS_HPP

#include "phlex/model/data_cell_index.hpp"

namespace phlex::test {
  inline auto make_run_index(int run_number)
  {
    return phlex::data_cell_index::job()->make_child("run", run_number);
  }
}

#endif // TEST_DATA_CELL_INDEX_HELPERS_HPP

#ifndef TEST_FOLD_TEST_ALGORITHMS_HPP
#define TEST_FOLD_TEST_ALGORITHMS_HPP

#include "phlex/model/data_cell_index.hpp"

#include <atomic>

namespace phlex::test {
  inline void add(std::atomic<unsigned int>& counter, unsigned int number) { counter += number; }

  inline unsigned int provide_number(data_cell_index const& index) { return index.number(); }
}

#endif // TEST_FOLD_TEST_ALGORITHMS_HPP

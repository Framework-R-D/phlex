#include "phlex/module.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

using namespace phlex::experimental::test;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  define_algorithm<phlex::data_cell_index, simb::MCTruths>(m, config);
}

#include "phlex/module.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

using namespace phlex::experimental::test;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  define_algorithm<phlex::experimental::data_cell_id, simb::MCTruths>(m, config);
}

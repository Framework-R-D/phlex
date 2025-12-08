#include "phlex/module.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

#include <tuple>

using namespace phlex::experimental::test;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  using inputs = phlex::experimental::data_cell_index;
  using outputs = std::tuple<simb::MCTruths, beam::ProtoDUNEBeamEvents, sim::ProtoDUNEbeamsims>;
  define_algorithm<inputs, outputs>(m, config);
}

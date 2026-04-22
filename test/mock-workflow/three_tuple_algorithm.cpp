#include "phlex/module.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

#include <tuple>

using namespace phlex::experimental::test;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using inputs = phlex::data_cell_index;
  using outputs = std::tuple<simb::MCTruths, beam::ProtoDUNEBeamEvents, sim::ProtoDUNEbeamsims>;
  define_algorithm<inputs, outputs>(m, config);
}

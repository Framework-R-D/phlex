#include "phlex/module.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"
#include <tuple>

using namespace phlex::experimental::test;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  define_algorithm<sim::SimEnergyDeposits,
                   std::tuple<sim::SimPhotonLites, sim::OpDetBacktrackerRecords>>(m, config);
}

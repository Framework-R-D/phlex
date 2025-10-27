#include "phlex/module.hpp"
#include "phlex/utilities/sized_tuple.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

using namespace phlex::experimental::test;

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  define_algorithm<sim::SimEnergyDeposits,
                   phlex::experimental::sized_tuple<sim::SimEnergyDeposits, 2>>(m, config);
}

#include "phlex/module.hpp"
#include "phlex/utilities/sized_tuple.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

#include <tuple>

using namespace phlex::experimental::test;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  using assns =
    phlex::experimental::association<simb::MCParticle, simb::MCTruth, sim::GeneratedParticleInfo>;
  using input = phlex::experimental::sized_tuple<simb::MCTruths, 6>;
  using output = std::tuple<sim::ParticleAncestryMap,
                            assns,
                            sim::SimEnergyDeposits,
                            sim::AuxDetHits,
                            simb::MCParticles>;
  define_algorithm<input, output>(m, config);
}

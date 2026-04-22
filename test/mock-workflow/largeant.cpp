#include "phlex/module.hpp"
#include "phlex/utilities/sized_tuple.hpp"
#include "test/mock-workflow/algorithm.hpp"
#include "test/mock-workflow/types.hpp"

#include <tuple>

using namespace phlex::experimental::test;

// BOOST_DLL_ALIAS creates a non-const exported function pointer; required by the dynamic linker.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
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

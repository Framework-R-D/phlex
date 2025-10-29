#ifndef TEST_MOCK_WORKFLOW_TYPES_HPP
#define TEST_MOCK_WORKFLOW_TYPES_HPP

namespace phlex::experimental {
  template <typename A, typename B, typename D = void>
  struct association {};
}

#define DATA_PRODUCT(name)                                                                         \
  struct name {}

namespace beam {
  DATA_PRODUCT(ProtoDUNEBeamEvents);
}
namespace sim {
  DATA_PRODUCT(AuxDetHits);
  DATA_PRODUCT(GeneratedParticleInfo);
  DATA_PRODUCT(OpDetBacktrackerRecords);
  DATA_PRODUCT(ParticleAncestryMap);
  DATA_PRODUCT(ProtoDUNEbeamsims);
  DATA_PRODUCT(SimEnergyDeposits);
  DATA_PRODUCT(SimPhotonLites);
}
namespace simb {
  DATA_PRODUCT(MCTruth);
  DATA_PRODUCT(MCTruths);
  DATA_PRODUCT(MCParticle);
  DATA_PRODUCT(MCParticles);
}

#undef DATA_PRODUCT

#endif // TEST_MOCK_WORKFLOW_TYPES_HPP

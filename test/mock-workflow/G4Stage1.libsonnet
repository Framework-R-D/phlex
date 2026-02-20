local generators = import 'SinglesGen.libsonnet';
local ev = import 'event_product.libsonnet';

{
  largeant: {
    cpp: 'largeant',
    duration_usec: 156,  // Typical: 15662051
    inputs: [ev.creator_event_product(f, 'MCTruths') for f in std.objectFields(generators)],
    outputs: ['ParticleAncestryMap', 'Assns', 'SimEnergyDeposits', 'AuxDetHits', 'MCParticles'],
  },
}

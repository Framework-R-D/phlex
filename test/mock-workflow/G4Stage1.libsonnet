local generators = import 'SinglesGen.libsonnet';

{
  largeant: {
    plugin: 'largeant',
    duration_usec: 156, # Typical: 15662051
    inputs: [f + "/MCTruths" for f in std.objectFields(generators)],
    outputs: ["ParticleAncestryMap", "Assns", "SimEnergyDeposits", "AuxDetHits", "MCParticles"],
  }
}

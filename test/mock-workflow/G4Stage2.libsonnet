local g4stage1 = import 'G4Stage1.libsonnet';

{
  IonAndScint: {
    plugin: 'ion_and_scint',
    duration_usec: 546, # Typical: 5457973
    inputs: [f + "/SimEnergyDeposits" for f in std.objectFields(g4stage1)],
    outputs: ["SimEnergyDeposits", "SimEnergyDeposits_priorSCE"],
  },
  PDFastSim: {
    plugin: 'pd_fast_sim',
    duration_usec: 69, # Typical: 69681950
    inputs: ['SimEnergyDeposits_priorSCE'],
    outputs: ['SimPhotonLites', 'OpDetBacktrackerRecords'],
  }
}

local g4stage1 = import 'G4Stage1.libsonnet';
local ev = import 'event_product.libsonnet';

{
  IonAndScint: {
    cpp: 'ion_and_scint',
    duration_usec: 546,  // Typical: 5457973
    inputs: [ev.event_product(f + '/SimEnergyDeposits') for f in std.objectFields(g4stage1)],
    outputs: ['SimEnergyDeposits', 'SimEnergyDeposits_priorSCE'],
  },
  PDFastSim: {
    cpp: 'pd_fast_sim',
    duration_usec: 69,  // Typical: 69681950
    inputs: [ev.event_product('SimEnergyDeposits_priorSCE')],
    outputs: ['SimPhotonLites', 'OpDetBacktrackerRecords'],
  },
}

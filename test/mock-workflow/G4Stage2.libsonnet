local g4stage1 = import 'G4Stage1.libsonnet';
local ev = import 'event_product.libsonnet';

{
  IonAndScint: {
    cpp: 'ion_and_scint',
    duration_usec: 546,  // Typical: 5457973
<<<<<<< new-product-query-api
    inputs: [ev.creator_event_product(f, 'SimEnergyDeposits') for f in std.objectFields(g4stage1)],
=======
    inputs: [ev.event_product(f + '/SimEnergyDeposits') for f in std.objectFields(g4stage1)],
>>>>>>> main
    outputs: ['SimEnergyDeposits', 'SimEnergyDeposits_priorSCE'],
  },
  PDFastSim: {
    cpp: 'pd_fast_sim',
    duration_usec: 69,  // Typical: 69681950
<<<<<<< new-product-query-api
    inputs: [ev.creator_event_product('IonAndScint', 'SimEnergyDeposits_priorSCE')],
=======
    inputs: [ev.event_product('SimEnergyDeposits_priorSCE')],
>>>>>>> main
    outputs: ['SimPhotonLites', 'OpDetBacktrackerRecords'],
  },
}

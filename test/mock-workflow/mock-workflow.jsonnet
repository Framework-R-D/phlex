local singlesgen = import 'SinglesGen.libsonnet';
local g4stage1 = import 'G4Stage1.libsonnet';
local g4stage2 = import 'G4Stage2.libsonnet';

{
  source: {
    plugin: 'mock_workflow_source',
    n_events: 1,
  },
  modules: singlesgen + g4stage1 + g4stage2 {
    provider: {
      plugin: 'id_provider'
    }
  },
}

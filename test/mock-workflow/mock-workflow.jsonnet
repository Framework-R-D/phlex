local singlesgen = import 'SinglesGen.libsonnet';
local g4stage1 = import 'G4Stage1.libsonnet';
local g4stage2 = import 'G4Stage2.libsonnet';

{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 1 }
    }
  },
  sources: {
    provider: {
      plugin: 'id_provider'
    }
  },
  modules: singlesgen + g4stage1 + g4stage2,
}

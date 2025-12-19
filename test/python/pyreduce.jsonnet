{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 }
    }
  },
  sources: {
    provider: {
      plugin: 'cppsource4py',
    }
  },
  modules: {
    pyreduce: {
      plugin: 'pymodule',
      pyplugin: 'reducer',
      input: ['i', 'j'],
    },
    pyverify: {
      plugin: 'pymodule',
      pyplugin: 'verify',
      input: ['sum'],
      sum_total: 4,
    },
  },
}

{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 }
    }
  },
  source: {
    provider: {
      plugin: 'cppsource4py',
    }
  },
  modules: {
    pyadd: {
      plugin: 'pymodule',
      pyplugin: 'adder',
      input: ['i', 'j'],
      output: ['sum'],
    },
    pyverify: {
      plugin: 'pymodule',
      pyplugin: 'verify',
      input: ['sum'],
      sum_total: 1,
    },
  },
}

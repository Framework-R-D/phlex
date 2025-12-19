{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: "job", total: 10, starting_number: 1 }
    }
  },
  modules: {
    cppdriver: {
      plugin: 'cppdriver4py',
    },
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

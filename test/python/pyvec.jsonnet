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
    pysum: {
      plugin: 'pymodule',
      pyplugin: 'sumit',
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

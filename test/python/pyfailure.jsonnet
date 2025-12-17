{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 }
    }
  },
  modules: {
    cppdriver: {
      plugin: 'cppdriver4py',
    },
    pyadd: {
      plugin: 'pymodule',
      pyplugin: 'adder',
      #input: ['i', 'j'],   # commented out to cause a failure
      output: ['sum'],
    },
  },
}

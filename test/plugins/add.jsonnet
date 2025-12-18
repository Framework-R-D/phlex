{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: "job", total: 10, starting_number: 1 }
    }
  },
  modules: {
    add: {
      plugin: 'module',
    },
    output: {
      plugin: 'output',
    },
  },
}

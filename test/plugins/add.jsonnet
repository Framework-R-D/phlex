{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { parent: "job", total: 10, starting_number: 1 }
    }
  },
  sources: {
    provider: {
      plugin: 'ij_source'
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

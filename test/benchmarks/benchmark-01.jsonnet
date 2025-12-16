{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

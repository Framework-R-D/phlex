{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  modules: {
    read_id: {
      plugin: 'read_id',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

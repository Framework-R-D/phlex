{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  sources: {
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
  modules: {
    read_id: {
      plugin: 'read_id',
    },
  },
}

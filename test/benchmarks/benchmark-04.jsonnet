{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
    },
    read_index: {
      plugin: 'read_index',
      consumes: { product: 'a', layer: "event" }
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

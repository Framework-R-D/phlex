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
    a1_creator: {
      plugin: 'last_index',
      product_name: "a1"
    },
    a2_creator: {
      plugin: 'last_index',
      product_name: "a2"
    },
  },
}

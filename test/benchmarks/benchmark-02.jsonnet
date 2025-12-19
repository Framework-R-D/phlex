{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  sources: {
    provider: {
      cpp: 'benchmarks_provider'
    }
  },
  modules: {
    a1_creator: {
      cpp: 'last_index',
      product_name: "a1"
    },
    a2_creator: {
      cpp: 'last_index',
      product_name: "a2"
    },
  },
}

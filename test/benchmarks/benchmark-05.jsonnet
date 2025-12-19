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
    b_creator: {
      plugin: 'last_index',
      produces: 'b',
    },
    c_creator: {
      plugin: 'last_index',
      produces: 'c',
    },
    d: {
      plugin: 'verify_difference',
      expected: 0
    },
  },
}

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
    b_creator: {
      plugin: 'plus_one',
    },
    c_creator: {
      plugin: 'plus_101',
    },
    d: {
      plugin: 'verify_difference',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

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
  },
}

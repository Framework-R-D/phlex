{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
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

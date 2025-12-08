{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
    },
    read_index: {
      plugin: 'read_index',
      consumes: 'a'
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

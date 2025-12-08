{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  }, 
}

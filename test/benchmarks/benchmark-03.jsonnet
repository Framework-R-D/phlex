{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
  },
  modules: {
    read_id: {
      plugin: 'read_id',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

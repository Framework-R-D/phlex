{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
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
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

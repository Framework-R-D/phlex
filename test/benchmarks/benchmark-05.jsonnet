{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
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

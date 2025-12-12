{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
  },
  modules: {
    even_filter: {
      plugin: 'accept_even_ids',
      input: { product: 'id', layer: 'event' },
    },
    b_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      produces: 'b',
    },
    c_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      produces: 'c',
    },
    d: {
      plugin: 'verify_difference',
      expected: 0
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

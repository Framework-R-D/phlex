{
  source: {
    plugin: 'benchmarks_source',
    n_events: 100000
  },
  modules: {
    even_filter: {
      plugin: 'accept_even_ids',
      product_name: 'id',
    },
    b_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      product_name: 'b',
    },
    c_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      product_name: 'c',
    },
    d: {
      plugin: 'verify_difference',
      expected: 0
    },
  },
}

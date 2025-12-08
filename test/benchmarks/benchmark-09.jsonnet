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
    even_filter: {
      plugin: 'accept_even_numbers',
      consumes: 'a',
    },
    d: {
      plugin: 'read_index',
      when: ['even_filter:accept_even_numbers'],
      consumes: 'b',
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

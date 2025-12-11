local max_number = 100000;

{
  source: {
    plugin: 'benchmarks_source',
    n_events: max_number,
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
      produces: 'a',
    },
    even_filter: {
      plugin: 'accept_even_numbers',
      consumes: 'a',
    },
    fibonacci_filter: {
      plugin: 'accept_fibonacci_numbers',
      consumes: 'a',
      max_number: max_number,
    },
    d: {
      plugin: 'verify_even_fibonacci_numbers',
      when: ['even_filter:accept_even_numbers', 'fibonacci_filter:accept'],
      consumes: 'a',
      max_number: max_number,
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

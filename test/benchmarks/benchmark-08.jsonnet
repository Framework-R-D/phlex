local max_number = 100000;

{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: max_number },
    },
  },
  sources: {
    provider: {
      cpp: 'benchmarks_provider',
    },
  },
  modules: {
    a_creator: {
      cpp: 'last_index',
      produces: 'a',
    },
    even_filter: {
      cpp: 'accept_even_numbers',
      consumes: { creator: 'a_creator', suffix: 'a', layer: 'event' },
    },
    fibonacci_filter: {
      cpp: 'accept_fibonacci_numbers',
      consumes: { creator: 'a_creator', suffix: 'a', layer: 'event' },
      max_number: max_number,
    },
    d: {
      cpp: 'verify_even_fibonacci_numbers',
      experimental_when: ['even_filter:accept_even_numbers', 'fibonacci_filter:accept'],
      consumes: { creator: 'a_creator', suffix: 'a', layer: 'event' },
      max_number: max_number,
    },
  },
}

{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 100000 },
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
      input: { creator: 'a_creator', layer: 'event', suffix: 'a' },
    },
    b_creator: {
      cpp: 'plus_one',
      input: { creator: 'a_creator', layer: 'event', suffix: 'a' },
    },
    even_filter: {
      cpp: 'accept_even_numbers',
      consumes: { creator: 'a_creator', suffix: 'a', layer: 'event' },
    },
    d: {
      cpp: 'read_index',
      experimental_when: ['even_filter:accept_even_numbers'],
      consumes: { creator: 'b_creator', suffix: 'b', layer: 'event' },
    },
  },
}

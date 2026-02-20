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
    even_filter: {
      cpp: 'accept_even_ids',
      input: { creator: 'input', suffix: 'id', layer: 'event' },
    },
    b_creator: {
      cpp: 'last_index',
      experimental_when: ['even_filter:accept_even_ids'],
      produces: 'b',
    },
    c_creator: {
      cpp: 'last_index',
      experimental_when: ['even_filter:accept_even_ids'],
      produces: 'c',
    },
    d: {
      cpp: 'verify_difference',
      i: {
        creator: 'b_creator',
        layer: 'event',
        suffix: 'b',
      },
      j: {
        creator: 'c_creator',
        layer: 'event',
        suffix: 'c',
      },
      expected: 0,
    },
  },
}

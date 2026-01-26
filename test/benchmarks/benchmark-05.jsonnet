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
    b_creator: {
      cpp: 'last_index',
      produces: 'b',
    },
    c_creator: {
      cpp: 'last_index',
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

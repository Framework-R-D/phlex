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
    },
    b_creator: {
      cpp: 'plus_one',
      input: { creator: 'a_creator', layer: 'event', suffix: 'a' },
    },
    c_creator: {
      cpp: 'plus_101',
      input: { creator: 'a_creator', layer: 'event', suffix: 'a' },
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
    },
  },
}

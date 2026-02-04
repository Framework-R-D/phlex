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
    read_index: {
      cpp: 'read_index',
      consumes: {
        creator: 'a_creator',
        suffix: 'a',
        layer: 'event',
      },
    },
  },
}

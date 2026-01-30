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
    },
    c_creator: {
      cpp: 'plus_101',
    },
    d: {
      cpp: 'verify_difference',
    },
  },
}

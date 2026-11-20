{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 3 },
    },
  },
  sources: {
    provider: {
      py: 'test_samelayer',
    },
  },
  modules: {
    samelayer: {
      py: 'test_samelayer',
    },
  },
}

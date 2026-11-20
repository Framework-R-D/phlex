{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 1 },
    },
  },
  modules: {
    badlayer: {
      py: 'test_badlayer',
    },
  },
}

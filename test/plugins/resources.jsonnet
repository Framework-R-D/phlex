{
  resources: {
    test: {
      cpp: 'test_resources',
      value: 42,
    },
  },
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'ij_source',
    },
  },
  modules: {
    resources: {
      cpp: 'resources_module',
    },
  },
}

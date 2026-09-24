{
  resources: {
    resources_for_testing: {
      cpp: 'register_resources_for_testing',
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
    verify_resources: {
      cpp: 'verify_resources',
    },
  },
}

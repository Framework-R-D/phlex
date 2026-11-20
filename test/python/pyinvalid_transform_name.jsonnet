{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 1, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    invalid_transform_name: {
      py: 'invalid_transform_name',
    },
  },
}

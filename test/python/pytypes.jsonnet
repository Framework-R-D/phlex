{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    cppdriver: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pytypes: {
      py: 'basic_types',
      creator: 'input',
      layer: 'event',
    },
  },
}


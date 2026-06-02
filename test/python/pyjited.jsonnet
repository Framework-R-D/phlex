{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 100000, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pyadd: {
      py: 'jited',
      input: [
        {
          creator: 'input',
          layer: 'event',
          suffix: 'i',
        },
        {
          creator: 'input',
          layer: 'event',
          suffix: 'j',
        },
      ],
      output: ['sum'],
    },
  },
}

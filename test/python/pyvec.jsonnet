{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pysum: {
      py: 'sumit',
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
    pyverify: {
      py: 'verify',
      input: [
        {
          creator: 'sum_array',
          layer: 'event',
          suffix: 'sum',
        },
      ],
      sum_total: 1,
    },
  },
}

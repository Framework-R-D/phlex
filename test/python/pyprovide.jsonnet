{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      py: 'pysource4py',
    },
  },
  modules: {
    pyadd: {
      py: 'adder',
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
          creator: 'iadd',
          layer: 'event',
          suffix: 'sum',
        },
      ],
      sum_total: 1,
    },
  },
}

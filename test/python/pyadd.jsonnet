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
    pyadd: {
      py: 'adder',
      name: 'iadd',
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
    pyadd_layerless: {
      py: 'adder',
      name: 'iadd_layerless',
      input: [
        {
          creator: 'input',
          suffix: 'i',
        },
      ],
      output: ['sum_layerless'],
    },
    pyverify: {
      py: 'verify',
      operation: 'eq',
      input: [
        {
          creator: 'iadd',
          layer: 'event',
          suffix: 'sum',
        },
      ],
      sum_total: 1,
    },
    pyverify_nosuff: {
      py: 'verify',
      operation: 'eq',
      input: [
        {
          creator: 'iadd',
          layer: 'event',
        },
      ],
      sum_total: 1,
    },
    pyverify_layerless: {
      py: 'verify',
      operation: 'min',
      input: [
        {
          creator: 'iadd_layerless',
          suffix: 'sum_layerless',
        },
      ],
      sum_total: 3,
    },
  },
}

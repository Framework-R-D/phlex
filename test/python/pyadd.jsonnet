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
    pyadd_nocreator: {
      py: 'adder',
      name: 'iadd_nocreator',
      input: [
        {
          layer: 'event',
          suffix: 'i',
        },
      ],
      output: ['sum_nocreator'],
    },
    pyverify_nocreator: {
      py: 'verify',
      operation: 'min',
      input: [
        {
          layer: 'event',
          suffix: 'sum_nocreator',
        },
      ],
      sum_total: 3,
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
    pyadd_only_suffix: {
      py: 'adder',
      name: 'iadd_only_suffix',
      input: [
        {
          suffix: 'i',
        },
      ],
      output: ['sum_only_suffix'],
    },
    pyverify_only_suffix: {
      py: 'verify',
      operation: 'min',
      input: [
        {
          suffix: 'sum_only_suffix',
        },
      ],
      sum_total: 3,
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
  },
}

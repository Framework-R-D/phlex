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
    pyreduce: {
      py: 'reducer',
      input: [
        { creator: 'input', layer: 'event', suffix: 'i' },
        { creator: 'input', layer: 'event', suffix: 'j' },
      ],
    },
    pyverify: {
      py: 'verify',
      input: [{ creator: 'reduce', layer: 'event', suffix: 'sum' }],
      sum_total: 4,
    },
  },
}

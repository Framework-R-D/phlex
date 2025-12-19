{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 }
    }
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    }
  },
  modules: {
    pyreduce: {
      py: 'reducer',
      input: ['i', 'j'],
    },
    pyverify: {
      py: 'verify',
      input: ['sum'],
      sum_total: 4,
    },
  },
}

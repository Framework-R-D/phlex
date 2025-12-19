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
    pyadd: {
      py: 'adder',
      input: ['i', 'j'],
      output: ['sum'],
    },
    pyverify: {
      py: 'verify',
      input: ['sum'],
      sum_total: 1,
    },
  },
}

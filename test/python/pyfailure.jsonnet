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
      #input: ['i', 'j'],   # commented out to cause a failure
      output: ['sum'],
    },
  },
}

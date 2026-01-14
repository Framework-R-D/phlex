{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 1, starting_number: 1 }
    }
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    }
  },
  modules: {
    test_exception: {
      py: 'test_callbacks',
      mode: 'exception',
      input: ['i'],
      output: ['out'],
    }
  }
}

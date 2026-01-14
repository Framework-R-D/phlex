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
    test_bad_uint: {
      py: 'test_callbacks',
      mode: 'bad_uint',
      input: ['i'],
      output: ['out_uint'],
    }
  }
}

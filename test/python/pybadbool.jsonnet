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
    test_bad_bool: {
      py: 'test_callbacks',
      mode: 'bad_bool',
      input: ['i'],
      output: ['out_bool'],
    },
    verify_bool: {
      py: 'verify',
      input: ['out_bool'],
      expected_bool: true,
    }
  }
}

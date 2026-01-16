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
    test_mismatch: {
      py: 'test_callbacks',
      mode: 'mismatch',
      # Providing 3 inputs for a 2-arg function
      input: ['i', 'j', 'k'],
      output: ['sum_out'],
    }
  }
}

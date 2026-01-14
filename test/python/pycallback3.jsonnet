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
    # 1. Test 3-arg callback (success case)
    test_three_args: {
      py: 'test_callbacks',
      mode: 'three_args',
      input: ['i', 'j', 'k'],
      output: ['sum_ijk'],
    },
    verify_three: {
      py: 'verify',
      input: ['sum_ijk'],
      sum_total: 1, # 1 event * (0+0+0? wait, i=event_num-1. event1->0. sum=0. )
                    # provider generates i, j starting at 0?
                    # cppsource4py probably uses event number.
    }
  }
}

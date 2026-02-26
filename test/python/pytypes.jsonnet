{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    cppdriver: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pytypes: {
      py: 'test_types',
      input_float: [
        {creator: 'input', layer: 'event', suffix: 'f1'},
        {creator: 'input', layer: 'event', suffix: 'f2'},
      ],
      output_float: ['sum_f'],
      input_double: [
        {creator: 'input', layer: 'event', suffix: 'd1'},
        {creator: 'input', layer: 'event', suffix: 'd2'},
      ],
      output_double: ['sum_d'],
      input_uint: [
        {creator: 'input', layer: 'event', suffix: 'u1'},
        {creator: 'input', layer: 'event', suffix: 'u2'},
      ],
      output_uint: ['sum_u'],
      input_bool: [
        {creator: 'input', layer: 'event', suffix: 'b1'},
        {creator: 'input', layer: 'event', suffix: 'b2'},
      ],
      output_bool: ['and_b'],
      output_vfloat: ['vec_f'],
      output_vdouble: ['vec_d'],
    },
    verify_bool: {
      py: 'verify_extended',
      input_bool: ['and_b'],
      expected_bool: false,
    },
  },
}

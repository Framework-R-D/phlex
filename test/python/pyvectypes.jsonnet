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
    vectypes: {
      py: 'vectypes',
      input_int32: [
        {creator: 'input', layer: 'event', suffix: 'i'},
        {creator: 'input', layer: 'event', suffix: 'j'},
      ],
      output_int32: ['sum_int32'],
      input_uint32: [
        {creator: 'input', layer: 'event', suffix: 'u1'},
        {creator: 'input', layer: 'event', suffix: 'u2'},
      ],
      output_uint32: ['sum_uint32'],
      input_int64: [
        {creator: 'input', layer: 'event', suffix: 'l1'},
        {creator: 'input', layer: 'event', suffix: 'l2'},
      ],
      output_int64: ['sum_int64'],
      input_uint64: [
        {creator: 'input', layer: 'event', suffix: 'ul1'},
        {creator: 'input', layer: 'event', suffix: 'ul2'},
      ],
      output_uint64: ['sum_uint64'],
      input_float32: [
        {creator: 'input', layer: 'event', suffix: 'f1'},
        {creator: 'input', layer: 'event', suffix: 'f2'},
      ],
      output_float32: ['sum_float32'],
      input_float64: [
        {creator: 'input', layer: 'event', suffix: 'd1'},
        {creator: 'input', layer: 'event', suffix: 'd2'},
      ],
      output_float64: ['sum_float64'],
    },
    verify_int32: {
      py: 'verify_extended',
      input_int: ['sum_int32'],
      sum_total: 1,
    },
    verify_uint32: {
      py: 'verify_extended',
      input_uint: ['sum_uint32'],
      sum_total: 1,
    },
    verify_int64: {
      py: 'verify_extended',
      input_long: ['sum_int64'],
      sum_total: 1,
    },
    verify_uint64: {
      py: 'verify_extended',
      input_ulong: ['sum_uint64'],
      sum_total: 100,
    },
    verify_float32: {
      py: 'verify_extended',
      input_float: ['sum_float32'],
      sum_total: 1.0,
    },
    verify_double: {
      py: 'verify_extended',
      input_double: ['sum_float64'],
      sum_total: 1.0,
    },
  },
}

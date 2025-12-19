{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 }
    }
  },
  sources: {
    cppdriver: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pytypes: {
      py: 'test_types',
      input_float: ['f1', 'f2'],
      output_float: ['sum_f'],
      input_double: ['d1', 'd2'],
      output_double: ['sum_d'],
      input_uint: ['u1', 'u2'],
      output_uint: ['sum_u'],
      output_vfloat: ['vec_f'],
      output_vdouble: ['vec_d'],
    },
  },
}

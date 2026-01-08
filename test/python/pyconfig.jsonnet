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
    pyconfig: {
      py: 'all_config',
      input: ['i', 'j'],
      a_bool: false,
      an_int: -37,
      a_uint: 18446744073709551616,
      a_float: 3.1415,
      a_string: 'foo',
      some_bools: [false, true],
      some_ints: [-1, 42, -55],
      some_uints: [18446744073709551616, 29, 137],
      some_floats: [3.1415, 2.71828],
      some_strings: ['aap', 'noot', 'mies'],
      empty: [],
    },
  },
}

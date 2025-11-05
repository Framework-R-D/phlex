{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
  },
  modules: {
    pyadd: {
      pymodule: 'adder',
      pyalg: 'add',
      #hof: 'transform',
      input: ['i', 'j'],
      output: ['sum'],
      plugin: 'pymodule',
    },
    pyverify: {
      pymodule: 'verify',
      pyalg: 'assert_sum',
      #hof: 'observe',
      input: ['sum'],
      plugin: 'pymodule',
      sum_total: 1,
    },
  },
}

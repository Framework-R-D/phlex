{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: false,
  },
  modules: {
    pyadd: {
      plugin: 'pymodule',
      pymodule: 'adder',
      input: ['i', 'j'],
      output: ['sum'],
    },
    pyverify: {
      plugin: 'pymodule',
      pymodule: 'verify',
      input: ['sum'],
      sum_total: 1,
    },
  },
}

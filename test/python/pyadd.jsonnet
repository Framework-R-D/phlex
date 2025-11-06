{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: false,
  },
  modules: {
    pyadd: {
      plugin: 'pymodule',
      pyplugin: 'adder',
      input: ['i', 'j'],
      output: ['sum'],
    },
    pyverify: {
      plugin: 'pymodule',
      pyplugin: 'verify',
      input: ['sum'],
      sum_total: 1,
    },
  },
}

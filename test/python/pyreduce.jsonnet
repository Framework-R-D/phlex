{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: false,
  },
  modules: {
    pyreduce: {
      plugin: 'pymodule',
      pyplugin: 'reducer',
      input: ['i', 'j'],
    },
    pyverify: {
      plugin: 'pymodule',
      pyplugin: 'verify',
      input: ['sum'],
      sum_total: 3,
    },
  },
}

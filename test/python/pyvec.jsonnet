{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: true,
  },
  modules: {
    pysum: {
      plugin: 'pymodule',
      pymodule: 'sumit',
      input: ['coll_of_i'],
      output: ['sum'],
    },
    pyverify: {
      plugin: 'pymodule',
      pymodule: 'verify',
      input: ['sum'],
      sum_total: 55,
    },
  },
}

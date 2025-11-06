{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: true,
  },
  modules: {
    pysum: {
      pymodule: 'sumit',
      pyalg: 'sum_coll',
      #hof: 'transform',
      input: ['coll_of_i'],
      output: ['sum'],
      plugin: 'pymodule',
    },
    pyverify: {
      pymodule: 'verify',
      pyalg: 'assert_sum',
      #hof: 'observe',
      input: ['sum'],
      plugin: 'pymodule',
      sum_total: 55,
    },
  },
}

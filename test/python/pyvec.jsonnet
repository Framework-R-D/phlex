{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
    as_collection: true,
  },
  modules: {
    pysum: {
      plugin: 'pymodule',
      pyplugin: 'sumit',
      input: ['coll_of_i'],
      output: ['sum'],
    },
    pyverify: {
      plugin: 'pymodule',
      pyplugin: 'verify',
      input: ['sum'],
      sum_total: 55,
    },
  },
}

{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
  },
  modules: {
    pyadd: {
      pymodule: 'adder',
      pyalg: 'add',
      binder: 'cppyy',
      hof: 'transform',
      input: ['i', 'j'],
      output: ['sum'],
      plugin: 'pymodule',
    },
#    pyverify: {
#      pymodule: 'verify',
#      pyalg: 'sum10',
#      binder: 'cppyy',
#      hof: 'observe',
#      input: ['sum'],
#      plugin: 'pymodule',
#    },
  },
}

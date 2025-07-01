{
  source: {
    plugin: 'py_cppdriver',
    max_numbers: 10,
  },
  modules: {
    pyadd: {
      pymodule: 'adder',
      pyalg: 'add',
      input: ['i', 'j'],
      output: ['sum'],
      plugin: 'pymodule',
    },
  },
}

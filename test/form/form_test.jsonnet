{
  source: {
    plugin: 'source',
    max_numbers: 10,
  },
  modules: {
    add: {
      plugin: 'module',
    },
    form_output: {
      plugin: 'form_module',
      products: {
        sum: {},
      },
    },
  },
}
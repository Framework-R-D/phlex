{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 10 },
    },
  },
  sources: {
    provider: {
      cpp: 'ij_source',
    },
  },
  modules: {
    add: {
      cpp: 'module',
    },
    form_output: {
      cpp: 'form_module',
      products: ['sum'],
    },
  },
}

{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 10 }
    }
  },
  modules: {
    add: {
      plugin: 'module',
    },
    form_output: {
      plugin: 'form_module',
      products: ["sum"],
    },
  },
}
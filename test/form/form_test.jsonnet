{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 10 }
    }
  },
  sources: {
    provider: {
      plugin: 'ij_source'
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

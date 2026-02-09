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
      // FIXME: Should make it possible to *not* write products created by nodes.
      //        If 'i' and 'j' are omitted from the products sequence below, an error
      //        is encountered with the message: 'No configuration found for product: j'.
      products: ['sum', 'i', 'j'],
    },
  },
}

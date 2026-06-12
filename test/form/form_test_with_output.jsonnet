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
      // Products written by the form module
      products: ['sum', 'i', 'j'],
    },
  },

  // Optional outputs mapping: file -> products written into that file.
  // Some test runners/CLI invocations can pick this up to produce a concrete
  // ROOT output file for easy inspection (e.g. "output.root").
  outputs: [
    {
      file: 'output.root',
      products: ['sum', 'i', 'j'],
    },
  ],
}

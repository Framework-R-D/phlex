{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: "job", total: 10, starting_number: 1 }
    }
  },
  sources: {
    provider: {
      cpp: 'ij_source'
    }
  },
  modules: {
    add: {
      cpp: 'module',
    },
    output: {
      cpp: 'output',
    },
  },
}

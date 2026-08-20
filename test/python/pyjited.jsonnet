{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 100, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pyadd: {
      py: 'jited',
    },
  },
}

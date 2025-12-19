{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 1 }
    }
  },
  modules: {
    mismatch: {
      py: 'test_mismatch',
    }
  }
}

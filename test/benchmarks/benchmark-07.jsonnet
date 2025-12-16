{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  sources: {
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
  modules: {
    even_filter: {
      plugin: 'accept_even_ids',
      input: { product: 'id', layer: 'event' },
    },
    b_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      produces: 'b',
    },
    c_creator: {
      plugin: 'last_index',
      when: ['even_filter:accept_even_ids'],
      produces: 'c',
    },
    d: {
      plugin: 'verify_difference',
      expected: 0
    },
  },
}

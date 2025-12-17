{
  source: {
    plugin: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
    },
    b_creator: {
      plugin: 'plus_one',
    },
    even_filter: {
      plugin: 'accept_even_numbers',
      consumes: { product: 'a', layer: "event" }
    },
    d: {
      plugin: 'read_index',
      when: ['even_filter:accept_even_numbers'],
      consumes: { product: 'b', layer: "event" }
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

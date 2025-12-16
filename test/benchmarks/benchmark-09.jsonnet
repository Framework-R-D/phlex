{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { total: 100000 }
    }
  },
  sources: {
    provider: {
      cpp: 'benchmarks_provider'
    }
  },
  modules: {
    a_creator: {
      cpp: 'last_index',
    },
    b_creator: {
      cpp: 'plus_one',
    },
    even_filter: {
      cpp: 'accept_even_numbers',
      consumes: { product: 'a', layer: "event" }
    },
    d: {
      cpp: 'read_index',
      when: ['even_filter:accept_even_numbers'],
      consumes: { product: 'b', layer: "event" }
    },
  },
}

local max_number = 100000;

{
  driver: {
    plugin: 'generate_layers',
    layers: {
      event: { total: max_number }
    }
  },
  modules: {
    a_creator: {
      plugin: 'last_index',
      produces: 'a',
    },
    even_filter: {
      plugin: 'accept_even_numbers',
      consumes: { product: 'a', layer: 'event' }
    },
    fibonacci_filter: {
      plugin: 'accept_fibonacci_numbers',
      consumes: { product: 'a', layer: "event" },
      max_number: max_number,
    },
    d: {
      plugin: 'verify_even_fibonacci_numbers',
      when: ['even_filter:accept_even_numbers', 'fibonacci_filter:accept'],
      consumes: { product: 'a', layer: "event" },
      max_number: max_number,
    },
    provider: {
      plugin: 'benchmarks_provider'
    }
  },
}

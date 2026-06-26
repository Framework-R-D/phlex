{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 4, starting_number: 0 },
    },
  },
  sources: {
    standard_source: {
      cpp: 'generate_vector',
      n_time_ticks: 16,
      seed: 101,
      creator: 'cov_standard',
    },
    offset_source: {
      cpp: 'generate_vector',
      n_time_ticks: 16,
      seed: 202,
      mean: 2,
      creator: 'cov_offset',
    },
  },
  modules: {
    add_cov: {
      cpp: 'generate_vector',
      lhs_creator: 'cov_standard',
      rhs_creator: 'cov_offset',
    },
    output: {
      cpp: 'form_module',
      output_file: 'form_source_coverage_input.root',
      products: ['sums'],
    },
  },
}

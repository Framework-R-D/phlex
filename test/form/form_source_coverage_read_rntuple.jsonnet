{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 4, starting_number: 0 },
    },
  },
  sources: {
    sums_from_form: {
      cpp: 'form_source',
      technology: 'ROOT_RNTUPLE',
      input_file: 'form_source_coverage_input.root',
      plugin: 'add_cov',
      algorithm: 'add_wires',
      creator: 'add_cov',
      products: ['sums'],
    },
  },
  modules: {
    pass_sums: {
      cpp: 'passthrough_vector',
      input_creator: 'add_cov',
    },
    output: {
      cpp: 'form_module',
      technology: 'ROOT_RNTUPLE',
      output_file: 'form_source_coverage_readback.root',
      products: ['sums'],
    },
  },
}

{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 100, starting_number: 0 },
    },
  },
  sources: {
    form_standard_and_offset_source: {
      cpp: 'form_source',
      input_file: 'form_gaussian_write_test.root',
      algorithm: 'add_wires',
      plugin: 'add_standard_and_offset_wires',
      products: ['sums'],
      creator: 'add_standard_and_offset_wires',
      //advertised_creator: 'add_standard_and_offset_wires', // new form source
    },
    form_standard_and_wider_source: {
      cpp: 'form_source',
      input_file: 'form_gaussian_write_test.root',
      algorithm: 'add_wires',
      plugin: 'add_standard_and_wider_wires',
      products: ['sums'],
      creator: 'add_standard_and_wider_wires',
      //advertised_creator: 'add_standard_and_wider_wires', // new form source
    },
  },
  modules: {
    register_form_source_extra_types: {
      cpp: 'form_source_extra_types',
    },
    add_sums_of_wires: {
      cpp: 'generate_vector',
      lhs_creator: 'add_standard_and_offset_wires',
      rhs_creator: 'add_standard_and_wider_wires',
    },
    output: {
      cpp: 'form_module',
      output_file: 'form_gaussian_read_test.root',
      products: ['sums'],
    },
  },
}

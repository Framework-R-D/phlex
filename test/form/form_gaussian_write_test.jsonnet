local wire_size = 1024; # ~600MB file in afew seconds with 4 threads on a GPVM

{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 100, starting_number: 0 },
    },
  },
  sources: {
    standard_normal_wires_source: {
      cpp: 'generate_vector',
      n_time_ticks: wire_size,
      creator: 'standard_normal_wires',
    },
    offset_normal_wires_source: {
      cpp: 'generate_vector',
      n_time_ticks: wire_size,
      mean: 2,
      creator: 'offset_normal_wires',
    },
    wider_central_normal_wires_source: {
      cpp: 'generate_vector',
      n_time_ticks: wire_size,
      stddev: 10,
      creator: 'wider_central_normal_wires',
    },
  },
  modules: {
    add_standard_and_offset_wires: {
      cpp: 'generate_vector',
      lhs_creator: 'standard_normal_wires',
      rhs_creator: 'offset_normal_wires',
    },
    add_standard_and_wider_wires: {
      cpp: 'generate_vector',
      lhs_creator: 'standard_normal_wires',
      rhs_creator: 'wider_central_normal_wires',
    },
    output: {
      cpp: 'form_module',
      output_file: 'form_gaussian_write_test.root',
      products: ['sums'],
    },
  },
}

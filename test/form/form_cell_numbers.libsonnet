// Round trip of each event's own number through a single-layer (job -> event) file.
local layers(starting_number) = {
  event: { parent: 'job', total: 200, starting_number: starting_number },
};

{
  write(output_file, starting_number):: {
    driver: { cpp: 'generate_layers', layers: layers(starting_number) },
    sources: {
      numbers: { cpp: 'cell_number_vector', creator: 'numbers' },
    },
    modules: {
      stamper: { cpp: 'cell_number_vector', input_creator: 'numbers' },
      output: { cpp: 'form_module', output_file: output_file, products: ['cell_numbers'] },
    },
  },

  read(input_file, starting_number):: {
    driver: { cpp: 'generate_layers', layers: layers(starting_number) },
    sources: {
      form_in: {
        cpp: 'form_source',
        stage: 'test',
        input_file: input_file,
        plugin: 'stamper',
        algorithm: 'stamp',
        creator: 'stamper',
        products: ['cell_numbers'],
      },
    },
    modules: {
      check: { cpp: 'cell_number_vector', input_creator: 'stamper', check: true },
    },
  },
}

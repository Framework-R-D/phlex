{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 1, starting_number: 0 },
    },
  },
  sources: {
    sums_from_form: {
      cpp: 'form_source',
      stage: 'test',
      input_file: 'does_not_matter.root',
      creator: 'bad creator',
      products: ['sums'],
    },
  },
}

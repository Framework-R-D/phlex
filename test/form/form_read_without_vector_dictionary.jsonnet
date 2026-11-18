{
  driver: {
    cpp: 'generate_layers',
    layers: {
      apa: { parent: 'job', total: 40, starting_number: 0 },
    },
  },
  sources: {
    form_waveform_source: {
      cpp: 'form_source',
      stage: 'test',
      input_file: 'form_write_without_vector_dictionary_test.root',
      algorithm: 'test_waveforms',
      plugin: 'test_waveforms',
      products: ['test_waveforms'],
      technology: 'ROOT_RNTUPLE',
      creator: 'test_waveforms',
    },
  },
  modules: {
    print_waveforms: {
      cpp: 'generate_waveform',
    },
  },
}

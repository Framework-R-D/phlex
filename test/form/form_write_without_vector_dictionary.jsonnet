{
  driver: {
    cpp: 'generate_layers',
    layers: {
      wire: { parent: 'job', total: 1024, starting_number: 0 },
    },
  },
  sources: {
    test_waveforms: {
      cpp: 'generate_waveform',
    },
  },
  modules: {
    print_waveforms: {
      cpp: 'generate_waveform'
    },
    output: {
      cpp: 'form_module',
      output_file: 'form_write_without_vector_dictionary_test.root',
      technology: 'ROOT_RNTUPLE',
      products: ['test_waveforms',],
    },
  },
}

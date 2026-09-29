local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Invalid: single-output keys cannot be combined with 'outputs'.
      products: ['wireCandidates'],
      outputs: {
        roi: { output_file: 'dune_conflict.root', products: ['wireCandidates'] },
      },
    },
  },
}

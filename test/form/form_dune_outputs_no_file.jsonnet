local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Invalid: a nested output must name its file.
      outputs: {
        roi: { products: ['wireCandidates'] },
      },
    },
  },
}

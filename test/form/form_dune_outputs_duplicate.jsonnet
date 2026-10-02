local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Invalid: wireCandidates is configured twice for the same file and technology.
      outputs: {
        first: { output_file: 'dune_duplicate.root', products: ['wireCandidates'] },
        second: { output_file: 'dune_duplicate.root', products: ['wireCandidates', 'spillCandidates'] },
      },
    },
  },
}

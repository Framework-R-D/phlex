local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Single-output keys: every product in one TTree file.
      output_file: 'dune_legacy.root',
      products: ['hitCandidates', 'wireCandidates', 'spillCandidates'],
    },
  },
}

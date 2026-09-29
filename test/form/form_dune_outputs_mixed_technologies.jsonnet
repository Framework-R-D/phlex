local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // One file, two technologies. wireCandidates comes from the same (creator, stage) in both.
      outputs: {
        ttree: {
          output_file: 'dune_mixed.root',
          technology: 'ROOT_TTREE',
          products: ['hitCandidates', 'wireCandidates'],
        },
        rntuple: {
          output_file: 'dune_mixed.root',
          technology: 'ROOT_RNTUPLE',
          products: ['wireCandidates', 'spillCandidates'],
        },
      },
    },
  },
}

local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Two TTree files; wireCandidates goes to both. An output with no products writes nothing.
      outputs: {
        roi: { output_file: 'dune_roi.root', products: ['hitCandidates', 'wireCandidates'] },
        summary: { output_file: 'dune_summary.root', products: ['wireCandidates', 'spillCandidates'] },
        unused: { output_file: 'dune_unused.root', products: [] },
      },
    },
  },
}

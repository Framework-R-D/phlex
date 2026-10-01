local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Valid: nothing is configured, so nothing is written and no error is raised.
      output_file: 'dune_empty.root',
      products: [],
    },
  },
}

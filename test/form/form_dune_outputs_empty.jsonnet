local dune = import 'dune_example_hits.libsonnet';

dune {
  modules+: {
    output: {
      cpp: 'form_module',
      // Invalid: 'outputs' must contain at least one output.
      outputs: {},
    },
  },
}

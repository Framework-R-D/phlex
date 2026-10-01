// The DUNE hit example as a Phlex job: the dune_example_hits plugin provides and verifies the
// products; tests add a form_module named 'output'.
{
  stage: 'test',
  driver: {
    cpp: 'generate_layers',
    layers: {
      spill: { parent: 'job', total: 2 },
      wire: { parent: 'spill', total: 3 },
      roi: { parent: 'wire', total: 2 },
    },
  },
  sources: {
    dune_hits: { cpp: 'dune_example_hits' },
  },
  modules: {
    verify_hits: { cpp: 'dune_example_hits' },
  },
}

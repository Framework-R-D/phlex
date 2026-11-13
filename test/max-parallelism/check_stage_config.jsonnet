local base = import 'check_parallelism_default.jsonnet';

base {
  stage: 'configured_stage',
  modules+: {
    verify+: {
      expected_stage: 'configured_stage',
    },
  },
}

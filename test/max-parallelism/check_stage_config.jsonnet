local base = import 'check_parallelism_default.jsonnet';

base {
  stage: 'configured-stage',
  modules+: {
    verify+: {
      expected_stage: 'configured-stage',
    },
  },
}

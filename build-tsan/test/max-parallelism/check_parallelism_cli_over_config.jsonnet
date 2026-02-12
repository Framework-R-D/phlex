local base = import 'check_parallelism_default.jsonnet';

base {
  max_concurrency: 7,
  modules+: {
    verify+: {
      expected_parallelism: 9,
    },
  },
}

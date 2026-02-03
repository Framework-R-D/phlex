local base = import 'check_parallelism_default.jsonnet';

base {
  local concurrency = 7,
  max_concurrency: concurrency,
  modules+: {
    verify+: {
      expected_parallelism: concurrency,
    },
  },
}

local base = import 'check_parallelism_default.jsonnet';

base {
  modules+: {
    verify+: {
      expected_parallelism: 7,
    },
  },
}

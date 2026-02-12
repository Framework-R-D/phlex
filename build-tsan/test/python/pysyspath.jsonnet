{
  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 10, starting_number: 1 },
    },
  },
  sources: {
    provider: {
      cpp: 'cppsource4py',
    },
  },
  modules: {
    pysyspath: {
      py: 'check_sys_path',
      input: ['i'],
      venv: '/workspace/build-tsan/test/python/py_virtual_env',
    },
  },
}

#include <atomic>
#include <dlfcn.h>
#include <stdexcept>
#include <string>

#include "phlex/core/framework_graph.hpp"

#include "wrap.hpp"

#ifdef PHLEX_HAVE_NUMPY
#define PY_ARRAY_UNIQUE_SYMBOL phlex_ARRAY_API
#include <numpy/arrayobject.h>
#endif

using namespace phlex::experimental;

static bool initialize();

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  initialize();

  PyGILRAII g;

  std::string modname = config.get<std::string>("py");
  PyObject* mod = PyImport_ImportModule(modname.c_str());
  if (mod) {
    PyObject* reg = PyObject_GetAttrString(mod, "PHLEX_REGISTER_ALGORITHMS");
    if (reg) {
      PyObject* pym = wrap_module(&m);
      PyObject* pyconfig = wrap_configuration(&config);
      if (pym && pyconfig) {
        PyObject* res = PyObject_CallFunctionObjArgs(reg, pym, pyconfig, nullptr);
        Py_XDECREF(res);
      }
      Py_XDECREF(pyconfig);
      Py_XDECREF(pym);
      Py_DECREF(reg);
    }
    Py_DECREF(mod);
  }

  if (PyErr_Occurred()) {
    std::string error_msg;
    if (!msg_from_py_error(error_msg))
      error_msg = "Unknown python error";
    throw std::runtime_error(error_msg.c_str());
  }
}

#ifdef PHLEX_HAVE_NUMPY
static void import_numpy(bool control_interpreter)
{
  static std::atomic<bool> numpy_imported{false};
  if (!numpy_imported.exchange(true)) {
    if (_import_array() < 0) {
      PyErr_Print();
      if (control_interpreter)
        Py_Finalize();
      throw std::runtime_error("build with numpy support, but numpy not importable");
    }
  }
}
#endif

static bool initialize()
{
  if (Py_IsInitialized()) {
#ifdef PHLEX_HAVE_NUMPY
    import_numpy(false);
#endif
    return true;
  }

  // TODO: the Python library is already loaded (b/c it's linked with
  // this module), but its symbols need to be exposed globally to Python
  // extension modules such as ctypes, yet this module is loaded with
  // private visibility only. The workaround here locates the library and
  // reloads (the handle is leaked b/c there's no knowing when it needs
  // to be offloaded).
  void* addr = dlsym(RTLD_DEFAULT, "Py_IsInitialized");
  if (addr) {
    Dl_info info;
    if (dladdr(addr, &info) == 0 || info.dli_fname == 0 || info.dli_fname[0] == 0) {
      throw std::runtime_error("unable to determine linked libpython");
    }
    dlopen(info.dli_fname, RTLD_GLOBAL | RTLD_NOW);
  }

#if PY_VERSION_HEX < 0x03020000
  PyEval_InitThreads();
#endif
#if PY_VERSION_HEX < 0x03080000
  Py_Initialize();
#else
  PyConfig config;
  PyConfig_InitPythonConfig(&config);
  PyConfig_SetString(&config, &config.program_name, L"phlex");
  Py_InitializeFromConfig(&config);
#endif
#if PY_VERSION_HEX >= 0x03020000
#if PY_VERSION_HEX < 0x03090000
  PyEval_InitThreads();
#endif
#endif
  // try again to see if the interpreter is now initialized
  if (!Py_IsInitialized())
    throw std::runtime_error("Python can not be initialized");

#if PY_VERSION_HEX < 0x03080000
  // set the command line arguments on python's sys.argv
  wchar_t* argv[] = {const_cast<wchar_t*>(L"phlex")};
  PySys_SetArgv(sizeof(argv) / sizeof(argv[0]), argv);
#endif

  // add custom types
  if (PyType_Ready(&PhlexConfig_Type) < 0)
    return false;
  if (PyType_Ready(&PhlexModule_Type) < 0)
    return false;
  if (PyType_Ready(&PhlexLifeline_Type) < 0)
    return false;

  // load numpy (see also above, if already initialized)
#ifdef PHLEX_HAVE_NUMPY
  import_numpy(true);
#endif

  // TODO: the GIL should first be released on the main thread and this seems
  // to be the only place to do it. However, there is no equivalent place to
  // re-acquire it after the TBB runs are done, so normal shutdown of the
  // Python interpreter will not happen atm.
  static std::atomic<bool> gil_released{false};
  if (!gil_released.exchange(true)) {
    (void)PyEval_SaveThread(); // state not saved, as no place to restore
  }

  return true;
}

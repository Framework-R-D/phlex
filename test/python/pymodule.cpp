#include "phlex/module.hpp"

#include <Python.h>
#include <dlfcn.h>

using namespace phlex::experimental;

static bool Initialize()
{
  if (Py_IsInitialized())
    return true;

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
  } else {
    throw std::runtime_error("can not locate linked libpython");
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

  return true;
}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  Initialize();

  PyObject* registry = PyImport_ImportModule("registry");
  if (registry) {
    PyObject* reg = PyObject_GetAttrString(registry, "register");
    if (reg) {
      PyObject* pym = PyCapsule_New(&m, nullptr, nullptr);
      PyObject* pyconfig = PyCapsule_New((void*)&config, nullptr, nullptr);
      PyObject* res = PyObject_CallFunctionObjArgs(reg, pym, pyconfig, nullptr);
      Py_XDECREF(res);
      Py_DECREF(pyconfig);
      Py_DECREF(pym);
      Py_DECREF(reg);
    }
    Py_DECREF(registry);
  }

  if (PyErr_Occurred()) {
    std::string msg;
#if PY_VERSION_HEX < 0x30c000000
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    if (value) {
      PyObject* pymsg = PyObject_Str(value);
      msg = PyUnicode_AsUTF8(pymsg);
      Py_DECREF(pymsg);
    } else {
      msg = "unknown Python error occurred";
    }
    Py_XDECREF(traceback);
    Py_XDECREF(value);
    Py_XDECREF(type);
#else
    PyObject* exc = PyErr_GetRaisedException();
    if (exc) {
      PyObject* pymsg = PyObject_Str(exc);
      msg = PyUnicode_AsString(pymsg);
      Py_DECREF(pymsg);
      Py_DECREF(exc);
    }
#endif
    throw std::runtime_error(msg.c_str());
  }
}


// dict-like construct to access configuration from Python

// TODO: the current implementation of the configuration hides the iteration
// over the underlying object, thus preventing nice printout etc.

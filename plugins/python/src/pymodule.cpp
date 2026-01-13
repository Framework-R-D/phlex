#include <atomic>
#include <dlfcn.h>
#include <stdexcept>
#include <string>

#include "phlex/core/framework_graph.hpp"

#include "wrap.hpp"

#define PY_ARRAY_UNIQUE_SYMBOL phlex_ARRAY_API
#include <numpy/arrayobject.h>

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

static void add_cmake_prefix_paths_to_syspath(char const* cmake_prefix_path)
{
  std::string prefix_path_str(cmake_prefix_path);
  std::istringstream iss(prefix_path_str);
  std::string path_entry;
  std::vector<std::string> site_package_paths;

  // Build site-packages paths from each CMAKE_PREFIX_PATH token
  while (std::getline(iss, path_entry, ':')) {
    if (!path_entry.empty()) {
      std::string version_str =
        std::to_string(PY_MAJOR_VERSION) + "." + std::to_string(PY_MINOR_VERSION);
      std::string site_packages = path_entry + "/lib/python" + version_str + "/site-packages";
      site_package_paths.push_back(site_packages);
    }
  }

  // Add each site-packages path to sys.path
  if (site_package_paths.empty()) {
    return;
  }

  if (PyObject* sys = PyImport_ImportModule("sys")) {
    if (PyObject* sys_path = PyObject_GetAttrString(sys, "path")) {
      if (PyList_Check(sys_path)) {
        for (auto const& sp_path : site_package_paths) {
          if (PyObject* py_path = PyUnicode_FromString(sp_path.c_str())) {
            // Insert at beginning to prioritize these paths
            PyList_Insert(sys_path, 0, py_path);
            Py_DECREF(py_path);
          }
        }
      }
      Py_DECREF(sys_path);
    }
    Py_DECREF(sys);
  }
}

static bool initialize()
{
  if (Py_IsInitialized()) {
    import_numpy(false);
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

  PyConfig config;
  PyConfig_InitPythonConfig(&config);
  PyConfig_SetString(&config, &config.program_name, L"phlex");

  PyStatus status = Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);
  if (PyStatus_Exception(status)) {
    throw std::runtime_error("Python initialization failed");
  }

  // try again to see if the interpreter is now initialized
  if (!Py_IsInitialized())
    throw std::runtime_error("Python can not be initialized");

  // add custom types
  if (PyType_Ready(&PhlexConfig_Type) < 0)
    return false;
  if (PyType_Ready(&PhlexModule_Type) < 0)
    return false;
  if (PyType_Ready(&PhlexLifeline_Type) < 0)
    return false;

  // FIXME: Spack does not set PYTHONPATH or VIRTUAL_ENV, but it does set
  //        CMAKE_PREFIX_PATH. Add site-packages directories from CMAKE_PREFIX_PATH
  //        to sys.path so Python can find packages in Spack views.
  if (!std::getenv("VIRTUAL_ENV")) {
    if (char const* cmake_prefix_path = std::getenv("CMAKE_PREFIX_PATH")) {
      add_cmake_prefix_paths_to_syspath(cmake_prefix_path);
    }
  }

  // load numpy (see also above, if already initialized)
  import_numpy(true);

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

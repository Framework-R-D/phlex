#ifndef phlex_python_wrap_hpp
#define phlex_python_wrap_hpp

// =======================================================================================
//
// Registration type wrappers.
//
// Design rationale
// ================
//
// The C++ and Python registration mechanisms are tailored to each language (e.g. the
// discovery of algorithm signatures is rather different). Furthermore, the Python side
// has its own registration pythonized module. Thus, it is not necessary to expose the
// full C++ registration types on the Python side and for the sake of efficiency, these
// wrappers provide a minimalistic interface.
//
// =======================================================================================

#include "Python.h"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

namespace phlex::experimental {

  // Create dict-like access to the configuration from Python.
  PyObject* wrap_configuration(configuration const* config);

  // Python wrapper for Phlex configuration
  extern PyTypeObject PhlexConfig_Type;
  struct py_config_map;

  // Phlex' Module wrapper to register algorithms.
  typedef graph_proxy<void_tag> phlex_module_t;
  PyObject* wrap_module(phlex_module_t* mod);

  // Python wrapper for Phlex modules
  extern PyTypeObject PhlexModule_Type;
  struct py_phlex_module;
}

#endif // phlex_python_pymodule_hpp

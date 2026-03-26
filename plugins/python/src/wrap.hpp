#ifndef PLUGINS_PYTHON_SRC_WRAP_HPP
#define PLUGINS_PYTHON_SRC_WRAP_HPP

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

#include <memory>
#include <string>

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"
#include "phlex/model/data_cell_index.hpp"

namespace phlex::experimental {

  // Create dict-like access to the configuration from Python.
  PyObject* wrap_configuration(configuration const& config); // returns new reference
  extern PyTypeObject PhlexConfig_Type;
  struct py_config_map;

  // Phlex' module wrapper to register algorithms
  typedef module_graph_proxy<void_tag> phlex_module_t;
  PyObject* wrap_module(phlex_module_t& mod); // returns new reference
  extern PyTypeObject PhlexModule_Type;
  struct py_phlex_module;

  // Phlex' source wrapper to register providers
  typedef source_graph_proxy<void_tag> phlex_source_t;
  PyObject* wrap_source(phlex_source_t& src); // returns new reference
  extern PyTypeObject PhlexSource_Type;
  struct py_phlex_source;

  // Python wrapper for data cell indices (returns a new reference)
  PyObject* wrap_dci(data_cell_index const& dci);
  extern PyTypeObject PhlexDataCellIndex_Type;

  // Python wrapper for Phlex handles
  extern PyTypeObject PhlexLifeline_Type;
  // clang-format off
  struct py_lifeline {
    PyObject_HEAD
    PyObject* m_view;
    std::shared_ptr<void> m_source;
  };
  using py_lifeline_t = py_lifeline;
  // clang-format on

  // Error reporting helper.
  bool msg_from_py_error(std::string& msg, bool check_error = false);

  // RAII helper for GIL handling
  class PyGILRAII {
    PyGILState_STATE m_GILState;

  public:
    PyGILRAII() : m_GILState(PyGILState_Ensure()) {}
    ~PyGILRAII() { PyGILState_Release(m_GILState); }
  };

} // namespace phlex::experimental

#endif // PLUGINS_PYTHON_SRC_WRAP_HPP

#include <string>

#include "phlex/configuration.hpp"
#include "wrap.hpp"

using namespace phlex::experimental;

// Create a dict-like access to the configuration from Python.
struct phlex::experimental::py_config_map {
  PyObject_HEAD phlex::experimental::configuration const* ph_config;
};

PyObject* phlex::experimental::wrap_configuration(configuration const* config)
{
  if (!config) {
    PyErr_SetString(PyExc_ValueError, "provided configuration is null");
    return nullptr;
  }

  py_config_map* pyconfig = PyObject_New(py_config_map, &PhlexConfig_Type);
  pyconfig->ph_config = config;

  return (PyObject*)pyconfig;
}

static PyObject* pcm_subscript(py_config_map* pycmap, PyObject* args)
{
  // Retrieve a named configuration setting.
  //
  // Configuration is only accessible through templated lookups and it is up to
  // the caller to figure the correct one. Here, conversion is attempted in order,
  // unless an optional type is provided. On failure, the setting is returned as
  // a string.
  //
  // Since the configuration is read-only, values are cached in the implicit
  // dictionary such that they can continue to be inspected.
  //
  // Python arguments expected:
  //  name: the property to retrieve
  //  type: the type to cast to (optional) and one of: int, float (for C++
  //        double), or str (for C++ std::string)
  //        tuple as standin for std::vector
  //  coll: boolean, set to True if this is a collection of <type>

  PyObject *pyname = nullptr, *type = nullptr;
  int coll = 0;
  if (PyTuple_Check(args)) {
    if (!PyArg_ParseTuple(args, "U|Op:__getitem__", &pyname, &type, &coll))
      return nullptr;
  } else
    pyname = args;

  // cached lookup
#if PY_VERSION_HEX >= 0x030d0000
  PyObject* pyvalue = nullptr;
  PyObject_GetOptionalAttr((PyObject*)pycmap, pyname, &pyvalue);
#else
  PyObject* pyvalue = PyObject_GetAttr((PyObject*)pycmap, pyname);
  if (!pyvalue)
    PyErr_Clear();
#endif
  if (pyvalue)
    return pyvalue;

  std::string cname = PyUnicode_AsUTF8(pyname);

  // typed conversion if provided
  if (type == (PyObject*)&PyUnicode_Type) {
    if (!coll) {
      auto const& cvalue = pycmap->ph_config->get<std::string>(cname);
      pyvalue = PyUnicode_FromString(cvalue.c_str());
    } else {
      auto const& cvalue = pycmap->ph_config->get<std::vector<std::string>>(cname);
      pyvalue = PyTuple_New(cvalue.size());
      for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
        PyObject* item = PyUnicode_FromString(cvalue[i].c_str());
        PyTuple_SetItem(pyvalue, i, item);
      }
    }
  } else if (type == (PyObject*)&PyLong_Type) {
    if (!coll) {
      auto const& cvalue = pycmap->ph_config->get<long>(cname);
      pyvalue = PyLong_FromLong(cvalue);
      return pyvalue;
    } else {
      auto const& cvalue = pycmap->ph_config->get<std::vector<std::string>>(cname);
      pyvalue = PyTuple_New(cvalue.size());
      for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
        PyObject* item = PyLong_FromString(cvalue[i].c_str(), nullptr, 10);
        PyTuple_SetItem(pyvalue, i, item);
      }
    }
  }

  // untyped (guess) conversion
  if (!pyvalue) {
    try {
      auto const& cvalue = pycmap->ph_config->get<long>(cname);
      pyvalue = PyLong_FromLong(cvalue);
    } catch (std::runtime_error const&) {
    }
  }
  if (!pyvalue) {
    try {
      auto const& cvalue = pycmap->ph_config->get<std::string>(cname);
      pyvalue = PyUnicode_FromStringAndSize(cvalue.c_str(), cvalue.size());
    } catch (std::runtime_error const&) {
    }
  }

  // cache if found

  return pyvalue;
}

static PyMappingMethods pcm_as_mapping = {nullptr, (binaryfunc)pcm_subscript, nullptr};

// clang-format off
PyTypeObject phlex::experimental::PhlexConfig_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  (char*) "pyphlex.configuration",                   // tp_name
  sizeof(py_config_map),                             // tp_basicsize
  0,                                                 // tp_itemsize
  0,                                                 // tp_dealloc
  0,                                                 // tp_vectorcall_offset / tp_print
  0,                                                 // tp_getattr
  0,                                                 // tp_setattr
  0,                                                 // tp_as_async / tp_compare
  0,                                                 // tp_repr
  0,                                                 // tp_as_number
  0,                                                 // tp_as_sequence
  &pcm_as_mapping,                                   // tp_as_mapping
  0,                                                 // tp_hash
  0,                                                 // tp_call
  0,                                                 // tp_str
  0,                                                 // tp_getattro
  0,                                                 // tp_setattro
  0,                                                 // tp_as_buffer
  Py_TPFLAGS_DEFAULT,                                // tp_flags
  (char*)"phlex configuration object-as-dictionary", // tp_doc
  0,                                                 // tp_traverse
  0,                                                 // tp_clear
  0,                                                 // tp_richcompare
  0,                                                 // tp_weaklistoffset
  0,                                                 // tp_iter
  0,                                                 // tp_iternext
  0,                                                 // tp_methods
  0,                                                 // tp_members
  0,                                                 // tp_getset
  0,                                                 // tp_base
  0,                                                 // tp_dict
  0,                                                 // tp_descr_get
  0,                                                 // tp_descr_set
  0,                                                 // tp_dictoffset
  0,                                                 // tp_init
  0,                                                 // tp_alloc
  0,                                                 // tp_new
  0,                                                 // tp_free
  0,                                                 // tp_is_gc
  0,                                                 // tp_bases
  0,                                                 // tp_mro
  0,                                                 // tp_cache
  0,                                                 // tp_subclasses
  0                                                  // tp_weaklist
#if PY_VERSION_HEX >= 0x02030000
  , 0                                                // tp_del
#endif
#if PY_VERSION_HEX >= 0x02060000
  , 0                                                // tp_version_tag
#endif
#if PY_VERSION_HEX >= 0x03040000
  , 0                                                // tp_finalize
#endif
#if PY_VERSION_HEX >= 0x03080000
  , 0                                                // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
  , 0                                                // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
  , 0                                                // tp_versions_used
#endif
};
// clang-format on

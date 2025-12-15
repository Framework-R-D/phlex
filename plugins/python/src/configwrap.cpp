#include <cstdint>
#include <string>

#include "phlex/configuration.hpp"
#include "wrap.hpp"

using namespace phlex::experimental;

// Create a dict-like access to the configuration from Python.
// clang-format off
struct phlex::experimental::py_config_map {
  PyObject_HEAD
  phlex::experimental::configuration const* ph_config;
  PyObject* ph_config_cache;
};
// clang-format on

PyObject* phlex::experimental::wrap_configuration(configuration const* config)
{
  if (!config) {
    PyErr_SetString(PyExc_ValueError, "provided configuration is null");
    return nullptr;
  }

  py_config_map* pyconfig =
    (py_config_map*)PhlexConfig_Type.tp_new(&PhlexConfig_Type, nullptr, nullptr);

  pyconfig->ph_config = config;

  return (PyObject*)pyconfig;
}

//= CPyCppyy low level view construction/destruction =========================
static py_config_map* pcm_new(PyTypeObject* subtype, PyObject*, PyObject*)
{
    py_config_map* pcm = (py_config_map*)subtype->tp_alloc(subtype, 0);
    if (!pcm)
        return nullptr;

    pcm->ph_config_cache = PyDict_New();

    return pcm;
}

static void pcm_dealloc(py_config_map* pcm)
{
  Py_DECREF(pcm->ph_config_cache);
  Py_TYPE(pcm)->tp_free((PyObject*)pcm);
}

static PyObject* pcm_subscript(py_config_map* pycmap, PyObject* pykey)
{
  // Retrieve a named configuration setting.
  //
  // Configuration should have a single in-memory representation, which is why
  // the current approach retrieves it from the equivalent C++ object, ie. after
  // the JSON input has been parsed, even as there are Python JSON parsers.
  //
  // pykey: the lookup key to retrieve the configuration value

  if (!PyUnicode_Check(pykey)) {
    PyErr_SetString(PyExc_TypeError, "__getitem__ expects a string key");
    return nullptr;
  }

  // cached lookup
  PyObject* pyvalue = PyDict_GetItem(pycmap->ph_config_cache, pykey);
  if (pyvalue) {
    Py_INCREF(pyvalue);
    return pyvalue;
  }
  PyErr_Clear();

  std::string ckey = PyUnicode_AsUTF8(pykey);

  try {
    auto k = pycmap->ph_config->kind(ckey);
    if (k.second /* is array */) {
      if (k.first == boost::json::kind::bool_) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<bool>>(ckey);
        pyvalue = PyTuple_New(cvalue.size());
        for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
          PyObject* item = PyLong_FromLong((long)cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::int64) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::int64_t>>(ckey);
        pyvalue = PyTuple_New(cvalue.size());
        for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
          PyObject* item = PyLong_FromLong(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::uint64) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::uint64_t>>(ckey);
        pyvalue = PyTuple_New(cvalue.size());
        for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
          PyObject* item = PyLong_FromUnsignedLong(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::double_) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<double>>(ckey);
        pyvalue = PyTuple_New(cvalue.size());
        for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
          PyObject* item = PyFloat_FromDouble(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::string) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::string>>(ckey);
        pyvalue = PyTuple_New(cvalue.size());
        for (Py_ssize_t i = 0; i < (Py_ssize_t)cvalue.size(); ++i) {
          PyObject* item = PyUnicode_FromStringAndSize(cvalue[i].c_str(), cvalue[i].size());
          PyTuple_SetItem(pyvalue, i, item);
        }
      }
    } else {
      if (k.first == boost::json::kind::bool_) {
        auto cvalue = pycmap->ph_config->get<bool>(ckey);
        pyvalue = PyBool_FromLong((long)cvalue);
      } else if (k.first == boost::json::kind::int64) {
        auto cvalue = pycmap->ph_config->get<std::int64_t>(ckey);
        pyvalue = PyLong_FromLong(cvalue);
      } else if (k.first == boost::json::kind::uint64) {
        auto cvalue = pycmap->ph_config->get<std::uint64_t>(ckey);
        pyvalue = PyLong_FromUnsignedLong(cvalue);
      } else if (k.first == boost::json::kind::double_) {
        auto cvalue = pycmap->ph_config->get<double>(ckey);
        pyvalue = PyFloat_FromDouble(cvalue);
      } else if (k.first == boost::json::kind::string) {
        auto const& cvalue = pycmap->ph_config->get<std::string>(ckey);
        pyvalue = PyUnicode_FromStringAndSize(cvalue.c_str(), cvalue.size());
      }
    }
  } catch (std::runtime_error const&) {
    PyErr_Format(PyExc_TypeError, "property \"%s\" does not exist", ckey.c_str());
  }

  // cache if found
  if (pyvalue) {
    PyDict_SetItem(pycmap->ph_config_cache, pykey, pyvalue);
  }

  return pyvalue;
}

static PyMappingMethods pcm_as_mapping = {nullptr, (binaryfunc)pcm_subscript, nullptr};

// clang-format off
PyTypeObject phlex::experimental::PhlexConfig_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  (char*) "pyphlex.configuration",                   // tp_name
  sizeof(py_config_map),                             // tp_basicsize
  0,                                                 // tp_itemsize
  (destructor)pcm_dealloc,                           // tp_dealloc
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
  offsetof(py_config_map, ph_config_cache),          // tp_dictoffset
  0,                                                 // tp_init
  0,                                                 // tp_alloc
  (newfunc)pcm_new,                                  // tp_new
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

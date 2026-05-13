#include <cstdint>
#include <optional>
#include <ranges>
#include <string>

#include "phlex/configuration.hpp"
#include "wrap.hpp"

using namespace phlex::experimental;

// Create a dict-like access to the configuration from Python.
// clang-format off
struct phlex::experimental::py_config_map {
  PyObject_HEAD
  phlex::configuration const* ph_config;
  PyObject* ph_config_cache;
};
// clang-format on

PyObject* phlex::experimental::wrap_configuration(configuration const& config)
{
  py_config_map* pyconfig =
    reinterpret_cast<py_config_map*>(PhlexConfig_Type.tp_new(&PhlexConfig_Type, nullptr, nullptr));

  pyconfig->ph_config = &config;

  return reinterpret_cast<PyObject*>(pyconfig);
}

static py_config_map* pcm_new(PyTypeObject* subtype, PyObject*, PyObject*)
{
  py_config_map* pcm = reinterpret_cast<py_config_map*>(subtype->tp_alloc(subtype, 0));
  if (!pcm)
    return nullptr;

  pcm->ph_config_cache = PyDict_New();

  return pcm;
}

static void pcm_dealloc(py_config_map* pcm)
{
  Py_DECREF(pcm->ph_config_cache);
  Py_TYPE(pcm)->tp_free(reinterpret_cast<PyObject*>(pcm));
}

// Returns the array size as Py_ssize_t, or std::nullopt (and sets a Python
// OverflowError) if the size exceeds PY_SSIZE_T_MAX.
static std::optional<Py_ssize_t> checked_tuple_size(std::size_t n)
{
  if (n > static_cast<std::size_t>(PY_SSIZE_T_MAX)) {
    PyErr_Format(PyExc_OverflowError, "array is too large to convert to a Python tuple");
    return std::nullopt;
  }
  return static_cast<Py_ssize_t>(n);
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

  // Note: Python3.14 adds PyLong_FromInt64/PyLong_FromUInt64 to replace the
  // long long variants
  static_assert(sizeof(long long) >= sizeof(int64_t));
  static_assert(sizeof(unsigned long long) >= sizeof(uint64_t));

  try {
    auto k = pycmap->ph_config->prototype_internal_kind(ckey);
    if (k.second /* is array */) {
      if (k.first == boost::json::kind::bool_) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<bool>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          PyObject* item = PyLong_FromLong((long)cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::int64) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::int64_t>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          // Note Python3.14 is expected to add PyLong_FromInt64
          PyObject* item = PyLong_FromLongLong(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::uint64) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::uint64_t>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          // Note Python3.14 is expected to add PyLong_FromUInt64
          PyObject* item = PyLong_FromUnsignedLongLong(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::double_) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<double>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          PyObject* item = PyFloat_FromDouble(cvalue[i]);
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::string) {
        auto const& cvalue = pycmap->ph_config->get<std::vector<std::string>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          PyObject* item =
            PyUnicode_FromStringAndSize(cvalue[i].c_str(), (Py_ssize_t)cvalue[i].size());
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::object) {
        auto const& cvalue =
          pycmap->ph_config->get<std::vector<std::map<std::string, std::string>>>(ckey);
        auto const cvalue_size = checked_tuple_size(cvalue.size());
        if (!cvalue_size)
          return nullptr;
        pyvalue = PyTuple_New(*cvalue_size);
        // We can use std::views::enumerate once the AppleClang C++ STL supports it.
        for (Py_ssize_t i = 0; i < *cvalue_size; ++i) {
          PyObject* item = PyDict_New();
          for (auto const& kv : cvalue[i]) {
            PyObject* val =
              PyUnicode_FromStringAndSize(kv.second.c_str(), (Py_ssize_t)kv.second.size());
            PyDict_SetItemString(item, kv.first.c_str(), val);
            Py_DECREF(val);
          }
          PyTuple_SetItem(pyvalue, i, item);
        }
      } else if (k.first == boost::json::kind::null) {
        // special case: empty array
        pyvalue = PyTuple_New(0);
      }
    } else {
      if (k.first == boost::json::kind::bool_) {
        auto cvalue = pycmap->ph_config->get<bool>(ckey);
        pyvalue = PyBool_FromLong((long)cvalue);
      } else if (k.first == boost::json::kind::int64) {
        auto cvalue = pycmap->ph_config->get<std::int64_t>(ckey);
        // Note Python3.14 is expected to add PyLong_FromInt64
        pyvalue = PyLong_FromLongLong(cvalue);
      } else if (k.first == boost::json::kind::uint64) {
        auto cvalue = pycmap->ph_config->get<std::uint64_t>(ckey);
        // Note Python3.14 is expected to add PyLong_FromUInt64
        pyvalue = PyLong_FromUnsignedLongLong(cvalue);
      } else if (k.first == boost::json::kind::double_) {
        auto cvalue = pycmap->ph_config->get<double>(ckey);
        pyvalue = PyFloat_FromDouble(cvalue);
      } else if (k.first == boost::json::kind::string) {
        auto const& cvalue = pycmap->ph_config->get<std::string>(ckey);
        pyvalue = PyUnicode_FromStringAndSize(cvalue.c_str(), (Py_ssize_t)cvalue.size());
      } else if (k.first == boost::json::kind::object) {
        auto cvalue = pycmap->ph_config->get<std::map<std::string, std::string>>(ckey);
        pyvalue = PyDict_New();
        for (auto const& kv : cvalue) {
          PyObject* val =
            PyUnicode_FromStringAndSize(kv.second.c_str(), (Py_ssize_t)kv.second.size());
          PyDict_SetItemString(pyvalue, kv.first.c_str(), val);
          Py_DECREF(val);
        }
      }
    }
  } catch (std::runtime_error const& e) {
    PyErr_Format(PyExc_KeyError, "failed to retrieve property \"%s\" (%s)", ckey.c_str(), e.what());
  }

  // cache if found
  if (pyvalue) {
    PyDict_SetItem(pycmap->ph_config_cache, pykey, pyvalue);
  } else if (!PyErr_Occurred()) {
    PyErr_Format(PyExc_KeyError, "property \"%s\" is of unknown type", ckey.c_str());
  }

  return pyvalue;
}

// PyMappingMethods must be non-const; tp_as_mapping in PyTypeObject takes a non-const pointer.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static PyMappingMethods pcm_as_mapping = {
  nullptr, reinterpret_cast<binaryfunc>(pcm_subscript), nullptr};

// clang-format off
// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::PhlexConfig_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "pyphlex.configuration",                           // tp_name
  sizeof(py_config_map),                             // tp_basicsize
  0,                                                 // tp_itemsize
  reinterpret_cast<destructor>(pcm_dealloc),         // tp_dealloc
  0,                                                 // tp_vectorcall_offset / tp_print
  nullptr,                                           // tp_getattr
  nullptr,                                           // tp_setattr
  nullptr,                                           // tp_as_async / tp_compare
  nullptr,                                           // tp_repr
  nullptr,                                           // tp_as_number
  nullptr,                                           // tp_as_sequence
  &pcm_as_mapping,                                   // tp_as_mapping
  nullptr,                                           // tp_hash
  nullptr,                                           // tp_call
  nullptr,                                           // tp_str
  nullptr,                                           // tp_getattro
  nullptr,                                           // tp_setattro
  nullptr,                                           // tp_as_buffer
  Py_TPFLAGS_DEFAULT,                                // tp_flags
  "phlex configuration object-as-dictionary",        // tp_doc
  nullptr,                                           // tp_traverse
  nullptr,                                           // tp_clear
  nullptr,                                           // tp_richcompare
  0,                                                 // tp_weaklistoffset
  nullptr,                                           // tp_iter
  nullptr,                                           // tp_iternext
  nullptr,                                           // tp_methods
  nullptr,                                           // tp_members
  nullptr,                                           // tp_getset
  nullptr,                                           // tp_base
  nullptr,                                           // tp_dict
  nullptr,                                           // tp_descr_get
  nullptr,                                           // tp_descr_set
  offsetof(py_config_map, ph_config_cache),          // tp_dictoffset
  nullptr,                                           // tp_init
  nullptr,                                           // tp_alloc
  reinterpret_cast<newfunc>(pcm_new),                // tp_new
  nullptr,                                           // tp_free
  nullptr,                                           // tp_is_gc
  nullptr,                                           // tp_bases
  nullptr,                                           // tp_mro
  nullptr,                                           // tp_cache
  nullptr,                                           // tp_subclasses
  nullptr                                            // tp_weaklist
#if PY_VERSION_HEX >= 0x02030000
  , nullptr                                          // tp_del
#endif
#if PY_VERSION_HEX >= 0x02060000
  , 0                                                // tp_version_tag
#endif
#if PY_VERSION_HEX >= 0x03040000
  , nullptr                                          // tp_finalize
#endif
#if PY_VERSION_HEX >= 0x03080000
  , nullptr                                          // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
  , 0                                                // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
  , 0                                                // tp_versions_used
#endif
};
// clang-format on

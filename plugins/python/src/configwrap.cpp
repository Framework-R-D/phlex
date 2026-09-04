#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

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
  auto* pyconfig = reinterpret_cast<py_config_map*>(
    phlex_config_type.tp_new(&phlex_config_type, nullptr, nullptr));

  pyconfig->ph_config = &config;

  return reinterpret_cast<PyObject*>(pyconfig);
}

static py_config_map* pcm_new(PyTypeObject* subtype, PyObject*, PyObject*)
{
  auto* pcm = reinterpret_cast<py_config_map*>(subtype->tp_alloc(subtype, 0));
  if (!pcm) {
    return nullptr;
  }

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

static PyObject* string_map_to_python(std::map<std::string, std::string> const& value)
{
  PyObject* result = PyDict_New();
  if (!result) {
    return nullptr;
  }

  for (auto const& [key, item] : value) {
    PyObject* pyitem =
      PyUnicode_FromStringAndSize(item.c_str(), static_cast<Py_ssize_t>(item.size()));
    if (!pyitem) {
      Py_DECREF(result);
      return nullptr;
    }
    int const status = PyDict_SetItemString(result, key.c_str(), pyitem);
    Py_DECREF(pyitem);
    if (status < 0) {
      Py_DECREF(result);
      return nullptr;
    }
  }
  return result;
}

template <typename T, typename Converter>
static PyObject* vector_to_python_tuple(std::vector<T> const& values, Converter&& convert)
{
  auto const size = checked_tuple_size(values.size());
  if (!size) {
    return nullptr;
  }

  PyObject* result = PyTuple_New(*size);
  if (!result) {
    return nullptr;
  }
  for (Py_ssize_t i = 0; i < *size; ++i) {
    PyObject* item = convert(values[static_cast<std::size_t>(i)]);
    // LCOV_EXCL_START
    if (!item) {
      // Practically speaking, this only happens when there's an allocation failure.
      Py_DECREF(result);
      return nullptr;
    }
    // LCOV_EXCL_STOP
    PyTuple_SET_ITEM(result, i, item);
  }
  return result;
}

static PyObject* configuration_array_to_python(phlex::configuration const& config,
                                               std::string const& key,
                                               boost::json::kind kind)
{
  if (kind == boost::json::kind::bool_) {
    auto const& value = config.get<std::vector<bool>>(key);
    return vector_to_python_tuple(
      value, [](bool item) { return PyBool_FromLong(static_cast<long>(item)); });
  }
  if (kind == boost::json::kind::int64) {
    auto const& value = config.get<std::vector<std::int64_t>>(key);
    return vector_to_python_tuple(value,
                                  [](std::int64_t item) { return PyLong_FromLongLong(item); });
  }
  if (kind == boost::json::kind::uint64) {
    auto const& value = config.get<std::vector<std::uint64_t>>(key);
    return vector_to_python_tuple(
      value, [](std::uint64_t item) { return PyLong_FromUnsignedLongLong(item); });
  }
  if (kind == boost::json::kind::double_) {
    auto const& value = config.get<std::vector<double>>(key);
    return vector_to_python_tuple(value, PyFloat_FromDouble);
  }
  if (kind == boost::json::kind::string) {
    auto const& value = config.get<std::vector<std::string>>(key);
    return vector_to_python_tuple(value, [](std::string const& item) {
      return PyUnicode_FromStringAndSize(item.c_str(), static_cast<Py_ssize_t>(item.size()));
    });
  }
  if (kind == boost::json::kind::object) {
    auto const& value = config.get<std::vector<std::map<std::string, std::string>>>(key);
    return vector_to_python_tuple(value, string_map_to_python);
  }
  if (kind == boost::json::kind::null) {
    return PyTuple_New(0);
  }
  return nullptr;
}

static PyObject* configuration_scalar_to_python(phlex::configuration const& config,
                                                std::string const& key,
                                                boost::json::kind kind)
{
  // Python 3.14 adds PyLong_FromInt64/PyLong_FromUInt64 to replace these variants.
  static_assert(sizeof(long long) >= sizeof(std::int64_t));
  static_assert(sizeof(unsigned long long) >= sizeof(std::uint64_t));

  if (kind == boost::json::kind::bool_) {
    return PyBool_FromLong(static_cast<long>(config.get<bool>(key)));
  }
  if (kind == boost::json::kind::int64) {
    return PyLong_FromLongLong(config.get<std::int64_t>(key));
  }
  if (kind == boost::json::kind::uint64) {
    return PyLong_FromUnsignedLongLong(config.get<std::uint64_t>(key));
  }
  if (kind == boost::json::kind::double_) {
    return PyFloat_FromDouble(config.get<double>(key));
  }
  if (kind == boost::json::kind::string) {
    auto const& value = config.get<std::string>(key);
    return PyUnicode_FromStringAndSize(value.c_str(), static_cast<Py_ssize_t>(value.size()));
  }
  if (kind == boost::json::kind::object) {
    return string_map_to_python(config.get<std::map<std::string, std::string>>(key));
  }
  return nullptr;
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
  PyObject* cached_value = PyDict_GetItem(pycmap->ph_config_cache, pykey);
  if (cached_value) {
    Py_INCREF(cached_value);
    return cached_value;
  }
  PyErr_Clear();

  char const* key_text = PyUnicode_AsUTF8(pykey);
  if (!key_text) {
    return nullptr;
  }
  std::string const ckey = key_text;

  PyObject* pyvalue = nullptr;
  try {
    auto const [kind, is_array] = pycmap->ph_config->prototype_internal_kind(ckey);
    pyvalue = is_array ? configuration_array_to_python(*pycmap->ph_config, ckey, kind)
                       : configuration_scalar_to_python(*pycmap->ph_config, ckey, kind);
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
static PyMappingMethods pcm_as_mapping = {.mp_length = nullptr,
                                          .mp_subscript =
                                            reinterpret_cast<binaryfunc>(pcm_subscript),
                                          .mp_ass_subscript = nullptr};

// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::phlex_config_type = {
  // clang-format off
  .ob_base = PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "pyphlex.configuration",
  // clang-format on
  .tp_basicsize = sizeof(py_config_map),
  .tp_itemsize = 0,
  .tp_dealloc = reinterpret_cast<destructor>(pcm_dealloc),
  .tp_vectorcall_offset = 0,
  .tp_getattr = nullptr,
  .tp_setattr = nullptr,
  .tp_as_async = nullptr,
  .tp_repr = nullptr,
  .tp_as_number = nullptr,
  .tp_as_sequence = nullptr,
  .tp_as_mapping = &pcm_as_mapping,
  .tp_hash = nullptr,
  .tp_call = nullptr,
  .tp_str = nullptr,
  .tp_getattro = nullptr,
  .tp_setattro = nullptr,
  .tp_as_buffer = nullptr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "phlex configuration object-as-dictionary",
  .tp_traverse = nullptr,
  .tp_clear = nullptr,
  .tp_richcompare = nullptr,
  .tp_weaklistoffset = 0,
  .tp_iter = nullptr,
  .tp_iternext = nullptr,
  .tp_methods = nullptr,
  .tp_members = nullptr,
  .tp_getset = nullptr,
  .tp_base = nullptr,
  .tp_dict = nullptr,
  .tp_descr_get = nullptr,
  .tp_descr_set = nullptr,
  .tp_dictoffset = offsetof(py_config_map, ph_config_cache),
  .tp_init = nullptr,
  .tp_alloc = nullptr,
  .tp_new = reinterpret_cast<newfunc>(pcm_new),
  .tp_free = nullptr,
  .tp_is_gc = nullptr,
  .tp_bases = nullptr,
  .tp_mro = nullptr,
  .tp_cache = nullptr,
  .tp_subclasses = nullptr,
  .tp_weaklist = nullptr,
  .tp_del = nullptr,
  .tp_version_tag = 0,
  .tp_finalize = nullptr,
  .tp_vectorcall = nullptr,
#if PY_VERSION_HEX >= 0x030c0000
  .tp_watched = 0,
#endif
#if PY_VERSION_HEX >= 0x030d0000
  .tp_versions_used = 0,
#endif
};

#include "dyncall.hpp"
#include "wrap.hpp"

#include "phlex/model/data_cell_index.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL phlex_ARRAY_API
#include <numpy/arrayobject.h>

// Python algorithms are supported by inserting nodes from C++ -> Python,
// followed by the intended call, and another from Python -> C++.
//
// Since product_selector inputs, list the creator name, the suffix can remain
// the same throughout the chain (as does the layer), distinguishing the
// stage with the creator name (and thus the node names) only.
//
// The chain is as follows (last step not added for observers):
//  C++ -> Python:   creator: <creator>
//                   name:    <name>_arg<N>_py
//                   output:  py_<suffix>
//  Python algoritm: creator: <name>_arg<N>>_py (xN)
//                   name:    py_<name>
//                   output:  <output>_py
//  Python -> C++:   creator: py_<name>
//                   name:    <name>
//                   output:  <output>
//
// For now, each input will have its own converter, even if multiple nodes
// need that same input translated. This simplifies memory management, but
// can cause a performance bottleneck (since all require the GIL).

// This is dumb, but for now, because all templates need to be instantiated, only
// support up to a fixed compile-time maximum number of arguments. An alternative
// would be to collect the arguments, but that currently suffers from needing a
// "initial" to create the container to collect arguments into. This may all go
// away once converter nodes have better support in phlex' core
constexpr size_t max_supported_args = 3;

using namespace phlex::experimental;
using namespace phlex;
using phlex::concurrency;
using phlex::product_selector;

// NOLINTBEGIN(performance-no-int-to-ptr) - necessary for Python interface

// Simple phlex module wrapper
// clang-format off
struct phlex::experimental::py_phlex_module {
  PyObject_HEAD
  phlex_module_t const* ph_module;
};
// clang-format on

PyObject* phlex::experimental::wrap_module(phlex_module_t const& module)
{
  py_phlex_module* pymod = PyObject_New(py_phlex_module, &phlex_module_type);
  pymod->ph_module = &module;

  return reinterpret_cast<PyObject*>(pymod);
}

// Simple phlex source wrapper
// clang-format off
struct phlex::experimental::py_phlex_source {
  PyObject_HEAD
  phlex_source_t const* ph_source;
};
// clang-format on

PyObject* phlex::experimental::wrap_source(phlex_source_t const& source)
{
  py_phlex_source* pysrc = PyObject_New(py_phlex_source, &phlex_source_type);
  pysrc->ph_source = &source;

  return reinterpret_cast<PyObject*>(pysrc);
}

namespace {

  inline std::string stringify(std::vector<std::string>& v) { return fmt::format("{:n}", v); }

  inline std::string stringify(std::vector<product_selector>& v)
  {
    return fmt::format("{:n}", std::ranges::views::transform(v, &product_selector::to_string));
  }

  inline std::string input_converter_name(std::string const& algname, size_t arg)
  {
    return fmt::format("{}_arg{}_py", algname, arg);
  }

  inline dcarg lifeline_transform(dcarg arg)
  {
    PyObject* pyobj = arg.get<PyObject*>();
    if (pyobj && PyObject_TypeCheck(pyobj, &phlex_lifeline_type)) {
      return dcarg{reinterpret_cast<py_lifeline_t*>(pyobj)->view};
    }
    return arg;
  }

  // callable objects managing the callback
  struct py_callback_base {
    PyObject* callable; // owned
    void* ccallback;    // C callable (either dispatcher or direct pointer)

    py_callback_base(PyObject* callable_object, void* cb) : callable(callable_object), ccallback(cb)
    {
      // callable is always non-null here (validated before construction)
      py_gilraii gil;
      Py_INCREF(callable);
    }
    py_callback_base(py_callback_base const& pc) : callable(pc.callable), ccallback(pc.ccallback)
    {
      // Must hold GIL when manipulating reference counts
      py_gilraii gil;
      Py_INCREF(callable);
    }
    py_callback_base& operator=(py_callback_base const& pc)
    {
      if (this != &pc) {
        py_gilraii gil;
        Py_INCREF(pc.callable);
        Py_DECREF(callable);
        callable = pc.callable;
        ccallback = pc.ccallback;
      }
      return *this;
    }
    py_callback_base(py_callback_base&& other) = delete;
    py_callback_base& operator=(py_callback_base&& other) = delete;
    // TODO: cleanup deferred to Phlex shutdown hook
    // Cannot safely Py_DECREF during arbitrary destruction due to:
    // - TOCTOU race on Py_IsInitialized() without GIL
    // - Module offloading in interpreter cleanup phase 2
    virtual ~py_callback_base() = default;
  };

  // type repeater to automatically instantiate callbacks taking N args
  template <typename T, size_t>
  using type_repeater = T;

  template <typename RT, typename Sq>
  struct py_callback_impl;

  template <typename RT, size_t... Is>
  struct py_callback_impl<RT, std::index_sequence<Is...>> : public py_callback_base {
    py_callback_impl(PyObject* callable) :
      py_callback_base(callable, reinterpret_cast<void*>(PyObject_CallFunctionObjArgs))
    {
    }

    RT operator()(type_repeater<dcarg, Is>... args)
    {
      dcargs_t argsv;
      argsv.reserve(sizeof...(Is) + 2);
      argsv.emplace_back(callable);
      (argsv.push_back(lifeline_transform(args)), ...);
      argsv.emplace_back(nullptr);

      py_gilraii gil;

      dcarg result{nullptr};
      dyncall(ccallback, result, argsv, 1);

      std::string error_msg;
      if (!result.get<PyObject*>()) {
        if (!msg_from_py_error(error_msg)) {
          error_msg = "Unknown python error";
        }
      }

      decref_all(args...);

      if (!error_msg.empty()) {
        throw std::runtime_error(error_msg);
      }

      if constexpr (!std::is_void_v<RT>) {
        return result;
      } else {
        Py_DECREF(result.get<PyObject*>());
      }
    }

  private:
    template <typename... Args>
    void decref_all(Args... args)
    {
      // helper to decrement reference counts of N arguments
      (Py_DECREF(reinterpret_cast<PyObject*>(std::get<void*>(args.value))), ...);
    }
  };

  template <typename RT, typename Sq>
  struct jit_callback_impl;

  template <typename RT, size_t... Is>
  struct jit_callback_impl<RT, std::index_sequence<Is...>> : public py_callback_base {
    dcarg rtype; // dynamic call return type

    jit_callback_impl(PyObject* callable, void* cb, std::string const& stype) :
      py_callback_base(callable, cb), rtype(dcarg::from_str(stype))
    {
    }

    RT operator()(type_repeater<dcarg, Is>... args)
    {
      dcarg result{rtype};
      dcargs_t argsv;
      argsv.reserve(sizeof...(Is));
      (argsv.push_back(args), ...);

      dyncall(ccallback, result, argsv);
      // TODO: error reporting?

      if constexpr (!std::is_void_v<RT>) {
        return result;
      }
    }
  };

  // aliases to reduce typing downstream (explicit instatiations used to ensure
  // that the function signature can be derived by the graph builder
  template <typename RT, size_t N>
  using py_callback = py_callback_impl<RT, std::make_index_sequence<N>>;

  template <typename RT, size_t N>
  using jit_callback = jit_callback_impl<RT, std::make_index_sequence<N>>;

  // input/output validation helpers
  inline std::optional<product_selector> validate_selector(PyObject* pysel)
  {
    if (!PyDict_Check(pysel)) {
      PyErr_Format(PyExc_TypeError, "selector should be a product specification");
      return std::nullopt;
    }

    PyObject* pyc = PyDict_GetItemString(pysel, "creator");
    if (!pyc || !PyUnicode_Check(pyc)) {
      PyErr_Format(PyExc_TypeError, "missing \"creator\" or not a string");
      return std::nullopt;
    }
    char const* c = PyUnicode_AsUTF8(pyc);

    PyObject* pyl = PyDict_GetItemString(pysel, "layer");
    if (!pyl || !PyUnicode_Check(pyl)) {
      PyErr_Format(PyExc_TypeError, "missing \"layer\" or not a string");
      return std::nullopt;
    }
    char const* l = PyUnicode_AsUTF8(pyl);

    std::optional<identifier> s;
    PyObject* pys = PyDict_GetItemString(pysel, "suffix");
    if (pys) {
      if (!PyUnicode_Check(pys)) {
        PyErr_Format(PyExc_TypeError, "provided \"suffix\" is not a string");
        return std::nullopt;
      }
      s = identifier(PyUnicode_AsUTF8(pys));
    } else {
      PyErr_Clear();
    }

    return std::optional<product_selector>{
      product_selector{.creator = identifier(c), .layer = identifier(l), .suffix = s}};
  }

  std::vector<product_selector> validate_input(PyObject* input)
  {
    std::vector<product_selector> cargs;
    if (!input) {
      return cargs;
    }

    PyObject* coll = PySequence_Fast(input, "input_family must be a sequence");
    if (!coll) {
      return cargs;
    }

    Py_ssize_t len = PySequence_Fast_GET_SIZE(coll);
    cargs.reserve(static_cast<size_t>(len));

    PyObject** items = PySequence_Fast_ITEMS(coll);
    for (Py_ssize_t i = 0; i < len; ++i) {
      PyObject* item = items[i]; // borrowed reference

      auto pq = validate_selector(item);
      if (pq.has_value()) {
        cargs.push_back(pq.value());
      } else {
        // validate_selector will have set a python exception
        break;
      }
    }

    Py_DECREF(coll);

    if (PyErr_Occurred()) {
      cargs.clear(); // error handled through Python
    }
    return cargs;
  }

  std::vector<std::string> validate_output(PyObject* output)
  {
    std::vector<std::string> cargs;
    if (!output) {
      return cargs;
    }

    PyObject* coll = PySequence_Fast(output, "output_product_suffixes must be a sequence");
    if (!coll) {
      return cargs;
    }

    Py_ssize_t len = PySequence_Fast_GET_SIZE(coll);
    cargs.reserve(static_cast<size_t>(len));

    PyObject** items = PySequence_Fast_ITEMS(coll);
    for (Py_ssize_t i = 0; i < len; ++i) {
      PyObject* item = items[i]; // borrowed reference
      if (!PyUnicode_Check(item)) {
        PyErr_Format(PyExc_TypeError, "item %d must be a string", static_cast<int>(i));
        break;
      }

      char const* p = PyUnicode_AsUTF8(item);
      if (!p) {
        break;
      }

      Py_ssize_t sz = PyUnicode_GetLength(item);
      cargs.emplace_back(p, static_cast<std::string::size_type>(sz));
    }

    Py_DECREF(coll);

    if (PyErr_Occurred()) {
      cargs.clear(); // error handled through Python
    }

    return cargs;
  }

} // unnamed namespace

namespace {

  bool is_numba_cfunc(PyObject* obj)
  {
    static PyObject* cfunc_type = nullptr;
    static bool checked = false;
    if (!checked) {
      checked = true;

      PyObject* nbmod = PyImport_ImportModule("numba.core.ccallback");
      if (nbmod) {
        cfunc_type = PyObject_GetAttrString(nbmod, "CFunc");
        Py_DECREF(nbmod);
      }

      if (!cfunc_type) {
        PyErr_Clear();
      }
      // hard reference to cfunc_type here if not null
    }

    if (!cfunc_type) {
      return false;
    }

    int result = PyObject_IsInstance(obj, cfunc_type);
    return result == 1;
  }

  std::string annotation_as_text(PyObject* pyobj)
  {
    static PyObject* normalizer = nullptr;

    // a return buffer is needed because the normalizer result is a Python
    // object that needs to be dereferenced before this function returns
    std::string result;

    if (!normalizer) {
      PyObject* phlexmod = PyImport_ImportModule("phlex");
      if (phlexmod) {
        normalizer = PyObject_GetAttrString(phlexmod, "normalize_type");
        Py_DECREF(phlexmod);
      }

      // LCOV_EXCL_START
      // this would only fail if the phlex installation were broken and
      // only exists to get a proper error message instead of a segfault
      // in that rather unlikely case
      if (!normalizer) {
        return result;
      }
      // LCOV_EXCL_STOP
    }

    PyObject* norm = PyObject_CallOneArg(normalizer, pyobj);
    if (!norm) {
      return result;
    }

    char const* ann = PyUnicode_AsUTF8(norm);
    if (ann) {
      result = ann;
    }
    Py_DECREF(norm);

    return result;
  }

  // retrieve C++ (matching) types from annotations
  bool annotations_to_strings(PyObject* callable,
                              std::vector<std::string>& input_types,
                              std::vector<std::string>& output_types)
  {
    bool conversion_ok = false;

    // TODO: move to PyObject_GetOptionalAttr and remove PyErr_Clear() in
    // the series of calls below, once we upgrade to py3.13
    PyObject* sann = PyUnicode_FromString("__annotations__");
    PyObject* annot = PyObject_GetAttr(callable, sann);
    if (!annot) {
      // the callable may be a Numba CFunc and have a declared signature
      PyErr_Clear();
      PyObject* sig = PyObject_GetAttrString(callable, "_sig");
      if (sig) {
        PyObject* ret = PyObject_GetAttrString(sig, "return_type");
        PyObject* args = PyObject_GetAttrString(sig, "args");

        if (ret && args && PyTuple_CheckExact(args)) {
          output_types.push_back(annotation_as_text(ret));
          for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(args); ++i) {
            PyObject* item = PyTuple_GET_ITEM(args, i);
            input_types.push_back(annotation_as_text(item));
          }
          conversion_ok = true;
        } else {
          PyErr_Clear();
        }

        Py_XDECREF(args);
        Py_XDECREF(ret);
        Py_DECREF(sig);
      } else {
        PyErr_Clear();
        // the callable may be an instance with a __call__ method
        PyObject* callm = PyObject_GetAttrString(callable, "__call__");
        if (callm) {
          annot = PyObject_GetAttr(callm, sann);
          Py_DECREF(callm);
        }
      }
    }
    Py_DECREF(sann);

    if (!conversion_ok && annot && PyDict_Check(annot)) {
      // Variant guarantees OrderedDict with "return" last
      Py_ssize_t pos = 0;

      PyObject* key = nullptr;
      PyObject* value = nullptr;

      conversion_ok = true;
      while (PyDict_Next(annot, &pos, &key, &value)) {
        std::string const& ann = annotation_as_text(value);
        if (ann.empty() && PyErr_Occurred()) {
          conversion_ok = false;
          break;
        }

        char const* key_str = PyUnicode_AsUTF8(key);
        if (strcmp(key_str, "return") == 0) {
          output_types.push_back(ann);
        } else {
          input_types.push_back(ann);
        }
      }
    } else {
      if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "unknown annotation formatting");
      }
    }

    Py_XDECREF(annot);
    return conversion_ok;
  }

  // converters of builtin types; TODO: this is a basic subset only, b/c either
  // these will be generated from the IDL, or they should be picked up from an
  // existing place such as cppyy

  bool pylong_as_bool(PyObject* pyobject)
  {
    // range-checking python integer to C++ bool conversion
    long l = PyLong_AsLong(pyobject);
    // fail to pass float -> bool; the problem is rounding (0.1 -> 0 -> False)
    if ((l != 0 && l != 1) || PyFloat_Check(pyobject)) {
      PyErr_SetString(PyExc_ValueError, "boolean value should be bool, or integer 1 or 0");
      return static_cast<bool>(-1);
    }
    return static_cast<bool>(l);
  }

  long pylong_as_strictlong(PyObject* pyobject)
  {
    // convert <pybject> to C++ long, don't allow truncation
    if (PyLong_Check(pyobject)) { // native Python integer
      return PyLong_AsLong(pyobject);
    }

    // accept numpy signed integer scalars (int8, int16, int32, int64)
    if (PyArray_IsScalar(pyobject, SignedInteger)) {
      // convert to Python int first, then to C long, that way we get a Python
      // OverflowError if out-of-range
      PyObject* pylong = PyNumber_Long(pyobject); // doesn't fail b/c of type check
      long result = PyLong_AsLong(pylong);
      Py_DECREF(pylong);
      return result;
    }

    PyErr_SetString(PyExc_TypeError, "int/long conversion expects a signed integer object");
    return static_cast<long>(-1);
  }

  unsigned long pylong_or_int_as_ulong(PyObject* pyobject)
  {
    // convert <pybject> to C++ unsigned long, with bounds checking, allow int -> ulong.
    if (PyFloat_Check(pyobject)) {
      PyErr_SetString(PyExc_TypeError, "can\'t convert float to unsigned long");
      return static_cast<unsigned long>(-1);
    }

    // accept numpy unsigned integer scalars (uint8, uint16, uint32, uint64)
    if (PyArray_IsScalar(pyobject, UnsignedInteger)) {
      // convert to Python int first, then to C unsigned long, that way we get a
      // Python OverflowError if out-of-range
      PyObject* pylong = PyNumber_Long(pyobject); // doesn't fail b/c of type check
      unsigned long result = PyLong_AsUnsignedLong(pylong);
      Py_DECREF(pylong);
      return result;
    }

    unsigned long ul = PyLong_AsUnsignedLong(pyobject);
    // In this case, using 'std::cmp_equal(ul, -1)' returns the wrong result, so
    // we suppress the clang-tidy warning re. 'ul == static_cast<unsigned long>(-1)'.
    // NOLINTNEXTLINE(modernize-use-integer-sign-comparison)
    if (ul == static_cast<unsigned long>(-1) && PyErr_Occurred() && PyLong_Check(pyobject)) {
      PyErr_Clear();
      long i = PyLong_AS_LONG(pyobject);
      if (0 <= i) {
        ul = static_cast<unsigned long>(i);
      } else {
        PyErr_SetString(PyExc_ValueError, "can\'t convert negative value to unsigned long");
        return static_cast<unsigned long>(-1);
      }
    }

    return ul;
  }

// NOLINTBEGIN(bugprone-macro-parentheses)
// `bugprone-macro-parentheses` expects macro parameters to be parenthesized. That is appropriate
// for expressions, but causes havoc with C++ signatures. We suppress this warning for the block
// because the use of continuations makes per-line suppression impossible.
#define BASIC_CONVERTER(name, cpptype, topy, frompy)                                               \
  static dcarg name##_to_py(cpptype a)                                                             \
  {                                                                                                \
    py_gilraii gil;                                                                                \
    return dcarg{topy(a)};                                                                         \
  }                                                                                                \
                                                                                                   \
  static dcarg name##_to_dcarg(cpptype a) { return dcarg{a}; }                                     \
                                                                                                   \
  static cpptype py_to_##name(dcarg a)                                                             \
  {                                                                                                \
    py_gilraii gil;                                                                                \
    PyObject* pyobj = a.get<PyObject*>();                                                          \
    auto i = static_cast<cpptype>(frompy(pyobj));                                                  \
    std::string msg;                                                                               \
    if (msg_from_py_error(msg, true)) {                                                            \
      Py_DECREF(pyobj);                                                                            \
      throw std::runtime_error("Python conversion error for type " #name ": " + msg);              \
    }                                                                                              \
    Py_DECREF(pyobj);                                                                              \
    return i;                                                                                      \
  }                                                                                                \
                                                                                                   \
  static cpptype dcarg_to_##name(dcarg a) { return a.get<cpptype>(); }                             \
                                                                                                   \
  struct provider_cb_##name : public py_callback<dcarg, 1> {                                       \
    using py_callback<dcarg, 1>::py_callback;                                                      \
    cpptype operator()(data_cell_index const& id)                                                  \
    {                                                                                              \
      py_gilraii gil;                                                                              \
      PyObject* arg0 = wrap_dci(id);                                                               \
      dcarg res = this->py_callback<dcarg, 1>::operator()(dcarg{arg0}); /* decrefs arg0 */         \
      PyObject* pyres = res.get<PyObject*>();                                                      \
      cpptype cres = frompy(pyres);                                                                \
      std::string msg;                                                                             \
      if (msg_from_py_error(msg, true)) {                                                          \
        Py_DECREF(pyres);                                                                          \
        throw std::runtime_error("Python provider conversion error for type " #name ": " + msg);   \
      }                                                                                            \
      Py_DECREF(pyres);                                                                            \
      return cres;                                                                                 \
    }                                                                                              \
  };

  BASIC_CONVERTER(bool, bool, PyBool_FromLong, pylong_as_bool)
  BASIC_CONVERTER(int, std::int32_t, PyLong_FromLong, PyLong_AsLong)
  BASIC_CONVERTER(uint, std::uint32_t, PyLong_FromLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(long, ph_long_t, PyLong_FromLong, pylong_as_strictlong)
  BASIC_CONVERTER(ulong, ph_ulong_t, PyLong_FromUnsignedLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(float, float, PyFloat_FromDouble, PyFloat_AsDouble)
  BASIC_CONVERTER(double, double, PyFloat_FromDouble, PyFloat_AsDouble)

#define VECTOR_CONVERTER(name, cpptype, nptype)                                                    \
  static dcarg name##_to_py(std::shared_ptr<std::vector<cpptype>> const& v)                        \
  {                                                                                                \
    py_gilraii gil;                                                                                \
                                                                                                   \
    if (!v) {                                                                                      \
      Py_INCREF(Py_None);                                                                          \
      return dcarg{Py_None};                                                                       \
    }                                                                                              \
                                                                                                   \
    /* use a numpy view with the shared pointer tied up in a lifeline object (note: this */        \
    /* is just a demonstrator; alternatives are still being considered) */                         \
    std::array<npy_intp, 1> dims{static_cast<npy_intp>(v->size())};                                \
                                                                                                   \
    PyObject* np_view = PyArray_SimpleNewFromData(1,           /* 1-D array */                     \
                                                  dims.data(), /* dimension sizes */               \
                                                  nptype,      /* numpy C type */                  \
                                                  (v->data())  /* raw buffer */                    \
    );                                                                                             \
                                                                                                   \
    if (!np_view) {                                                                                \
      throw std::runtime_error("failed to allocate numpy view object");                            \
    }                                                                                              \
                                                                                                   \
    /* make the data read-only by not making it writable */                                        \
    PyArray_CLEARFLAGS(reinterpret_cast<PyArrayObject*>(np_view), NPY_ARRAY_WRITEABLE);            \
                                                                                                   \
    /* create a lifeline object to tie this array and the original handle together; note */        \
    /* that the callback code needs to pick the data member out of the lifeline object, */         \
    /* when passing it to the registered Python function */                                        \
    py_lifeline_t* pyll = reinterpret_cast<py_lifeline_t*>(                                        \
      phlex_lifeline_type.tp_new(&phlex_lifeline_type, nullptr, nullptr));                         \
    if (!pyll) {                                                                                   \
      Py_DECREF(np_view);                                                                          \
      throw std::runtime_error("failed to allocate lifeline object");                              \
    }                                                                                              \
    pyll->source = v;                                                                              \
    pyll->view = np_view; /* steals reference */                                                   \
                                                                                                   \
    return dcarg{pyll};                                                                            \
  }

  VECTOR_CONVERTER(vint, std::int32_t, NPY_INT32)
  VECTOR_CONVERTER(vuint, std::uint32_t, NPY_UINT32)
  VECTOR_CONVERTER(vlong, std::int64_t, NPY_INT64)
  VECTOR_CONVERTER(vulong, std::uint64_t, NPY_UINT64)
  VECTOR_CONVERTER(vfloat, float, NPY_FLOAT)
  VECTOR_CONVERTER(vdouble, double, NPY_DOUBLE)

#define NUMPY_ARRAY_CONVERTER(name, cpptype, nptype, frompy)                                       \
  static std::shared_ptr<std::vector<cpptype>> py_to_##name(dcarg a)                               \
  {                                                                                                \
    py_gilraii gil;                                                                                \
                                                                                                   \
    auto vec = std::make_shared<std::vector<cpptype>>();                                           \
    PyObject* pyobj = a.get<PyObject*>();                                                          \
                                                                                                   \
    /* TODO: because of unresolved ownership issues, copy the full array contents */               \
    if (PyArray_Check(pyobj)) {                                                                    \
      PyArrayObject* arr = reinterpret_cast<PyArrayObject*>(pyobj);                                \
                                                                                                   \
      /* TODO: flattening the array here seems to be the only workable solution */                 \
      npy_intp* dims = PyArray_DIMS(arr);                                                          \
      int nd = PyArray_NDIM(arr);                                                                  \
      size_t total = 1;                                                                            \
      for (int i = 0; i < nd; ++i)                                                                 \
        total *= static_cast<size_t>(dims[i]);                                                     \
                                                                                                   \
      /* copy the array info; note that this assumes C continuity */                               \
      auto* raw = static_cast<cpptype*>(PyArray_DATA(arr));                                        \
      vec->reserve(total);                                                                         \
      vec->insert(vec->end(), raw, raw + total);                                                   \
    } else if (PyList_Check(pyobj)) {                                                              \
      Py_ssize_t total = PyList_Size(pyobj);                                                       \
      vec->reserve(total);                                                                         \
      for (Py_ssize_t i = 0; i < total; ++i) {                                                     \
        PyObject* item = PyList_GetItem(pyobj, i);                                                 \
        auto value = static_cast<cpptype>(frompy(item));                                           \
        std::string msg;                                                                           \
        if (msg_from_py_error(msg, true)) {                                                        \
          Py_DECREF(pyobj);                                                                        \
          throw std::runtime_error("List conversion error for type " #name ": " + msg);            \
        }                                                                                          \
        vec->push_back(value);                                                                     \
      }                                                                                            \
    } else {                                                                                       \
      std::string msg;                                                                             \
      if (msg_from_py_error(msg, true)) {                                                          \
        throw std::runtime_error("List conversion error: " + msg);                                 \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    Py_DECREF(pyobj);                                                                              \
    return vec;                                                                                    \
  }                                                                                                \
                                                                                                   \
  struct provider_cb_##name : public py_callback<dcarg, 1> {                                       \
    using py_callback<dcarg, 1>::py_callback;                                                      \
    std::shared_ptr<std::vector<cpptype>> operator()(data_cell_index const& id)                    \
    {                                                                                              \
      py_gilraii gil;                                                                              \
      PyObject* arg0 = wrap_dci(id);                                                               \
      dcarg pyres = this->py_callback<dcarg, 1>::operator()(dcarg{arg0}); /* decrefs arg0 */       \
      auto cres = py_to_##name(pyres);                                    /* decrefs pyres */      \
      return cres;                                                                                 \
    }                                                                                              \
  };
  // NOLINTEND(bugprone-macro-parentheses)

  NUMPY_ARRAY_CONVERTER(vint, std::int32_t, NPY_INT32, PyLong_AsLong)
  NUMPY_ARRAY_CONVERTER(vuint, std::uint32_t, NPY_UINT32, pylong_or_int_as_ulong)
  NUMPY_ARRAY_CONVERTER(vlong, std::int64_t, NPY_INT64, pylong_as_strictlong)
  NUMPY_ARRAY_CONVERTER(vulong, std::uint64_t, NPY_UINT64, pylong_or_int_as_ulong)
  NUMPY_ARRAY_CONVERTER(vfloat, float, NPY_FLOAT, PyFloat_AsDouble)
  NUMPY_ARRAY_CONVERTER(vdouble, double, NPY_DOUBLE, PyFloat_AsDouble)

  // helper for inserting converter nodes
  template <typename R, typename... Args>
  void insert_converter(py_phlex_module* mod,
                        std::string const& name,
                        R (*converter)(Args...),
                        product_selector pq_in,
                        std::string const& output,
                        concurrency nconcur)
  {
    mod->ph_module->transform(name, converter, nconcur)
      .input_family(std::move(pq_in))
      .output_product_suffixes(output);
  }

} // unnamed namespace

static PyObject* parse_args(PyObject* args,
                            PyObject* kwds,
                            std::string& functor_name,
                            std::vector<product_selector>& input_selectors,
                            std::vector<std::string>& input_types,
                            std::vector<std::string>& output_suffixes,
                            std::vector<std::string>& output_types,
                            concurrency& nconcur)
{
  // Helper function to extract the common names and identifiers needed to insert
  // any node. (The observer does not require outputs, but they still need to be
  // retrieved, not ignored, to issue an error message if an output is provided.)

  // Python 3.13+ accepts const keyword-name tables. Older versions require mutable strings.
#if PY_VERSION_HEX < 0x030d0000
  // NOLINTBEGIN(modernize-avoid-c-arrays)
  static char kw0[] = "callable";
  static char kw1[] = "input_family";
  static char kw2[] = "output_product_suffixes";
  static char kw3[] = "concurrency";
  static char kw4[] = "name";
  static char* kwnames[] = {kw0, kw1, kw2, kw3, kw4, nullptr};
  // NOLINTEND(modernize-avoid-c-arrays)
#else
  static std::array<char const*, 6> const kwnames{
    "callable", "input_family", "output_product_suffixes", "concurrency", "name", nullptr};
#endif
  PyObject* callable = nullptr;
  PyObject* input = nullptr;
  PyObject* output = nullptr;
  PyObject* pyname = nullptr;
  int nconcurrency = -1;
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "OO|OiO",
                                   std::data(kwnames),
                                   &callable,
                                   &input,
                                   &output,
                                   &nconcurrency,
                                   &pyname)) {
    // error already set by argument parser
    return nullptr;
  }

  if (!callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "provided algorithm is not callable");
    return nullptr;
  }

  // set concurrency, or the default of serial if not set
  nconcur = nconcurrency > 0 ? concurrency(nconcur) : concurrency::serial;

  // retrieve function name
  if (!pyname) {
    pyname = PyObject_GetAttrString(callable, "__name__");
    if (!pyname) {
      // AttributeError already set
      return nullptr;
    }
  } else {
    Py_INCREF(pyname);
  }

  functor_name = PyUnicode_AsUTF8(pyname);
  Py_DECREF(pyname);

  if (!input) {
    PyErr_SetString(PyExc_TypeError, "an input is required");
    return nullptr;
  }

  // convert input declarations, to be able to pass them to Phlex
  input_selectors = validate_input(input);
  if (input_selectors.empty()) {
    if (!PyErr_Occurred()) {
      PyErr_Format(PyExc_ValueError,
                   "no input provided for %s; node can not be scheduled",
                   functor_name.c_str());
    }
    return nullptr;
  }

  // convert output declarations, to be able to pass them to Phlex
  output_suffixes = validate_output(output);
  if (output_suffixes.size() > 1) {
    PyErr_SetString(PyExc_TypeError, "only a single output supported");
    return nullptr;
  }

  // retrieve C++ (matching) types if provided
  input_types.reserve(input_selectors.size());
  if (!annotations_to_strings(callable, input_types, output_types)) {
    return nullptr; // Python error already set
  }

  // ignore None as Python's conventional "void" return, which is meaningless in C++
  if (output_types.size() == 1 && output_types[0] == "None") {
    output_types.clear();
  }

  // if annotations were correct (and correctly parsed), there should be as many
  // input types as input product selectors
  if (input_types.size() != input_selectors.size()) {
    PyErr_Format(PyExc_TypeError,
                 "number of inputs (%d; %s) does not match number of annotation types (%d; %s)",
                 input_selectors.size(),
                 stringify(input_selectors).c_str(),
                 input_types.size(),
                 stringify(input_types).c_str());
    return nullptr;
  }

  // special case of Phlex Variant wrapper
  PyObject* wrapped_callable = PyObject_GetAttrString(callable, "phlex_callable");
  if (wrapped_callable) {
    // PyObject_GetAttrString returns a new reference, which we return
    callable = wrapped_callable;
  } else {
    // No wrapper, use the original callable with incremented reference count
    PyErr_Clear();
    Py_INCREF(callable);
  }

  // no common errors detected; actual registration may have more checks
  return callable;
}

// Returns the dtype suffix (e.g. "[float]") from a collection type string (e.g. "list[float]"),
// or std::nullopt if the string contains no '[' character.
static std::optional<std::string_view> collection_dtype(std::string const& type_name)
{
  auto const pos = type_name.rfind('[');
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  return std::string_view{type_name}.substr(pos);
}

static bool insert_input_converters(py_phlex_module* mod,
                                    std::string const& cname, // TODO: shared_ptr<PyObject>
                                    std::vector<product_selector> const& input_selectors,
                                    std::vector<std::string> const& input_types,
                                    bool ispy,
                                    concurrency nc)
{
  // insert input converter nodes into the graph
  // NOLINTNEXTLINE(readability-use-anyofallof): side effects and short-circuiting are intentional.
  for (auto const [i, inp_pq, inp_type] :
       std::views::zip(std::views::iota(size_t{}), input_selectors, input_types)) {
    // TODO: this seems overly verbose and inefficient, but the function needs
    // to be properly types, so every option is made explicit

    std::string const& pyname = input_converter_name(cname, i);
    std::string output =
      "py_" + (inp_pq.suffix ? std::string{static_cast<std::string_view>(*inp_pq.suffix)} : "");

    if (inp_type == "bool") {
      insert_converter(mod, pyname, ispy ? bool_to_py : bool_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "int32_t") {
      insert_converter(mod, pyname, ispy ? int_to_py : int_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "uint32_t") {
      insert_converter(mod, pyname, ispy ? uint_to_py : uint_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "int64_t") {
      insert_converter(mod, pyname, ispy ? long_to_py : long_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "uint64_t") {
      insert_converter(mod, pyname, ispy ? ulong_to_py : ulong_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "float") {
      insert_converter(mod, pyname, ispy ? float_to_py : float_to_dcarg, inp_pq, output, nc);
    } else if (inp_type == "double") {
      insert_converter(mod, pyname, ispy ? double_to_py : double_to_dcarg, inp_pq, output, nc);
    } else if (inp_type.starts_with("ndarray") || inp_type.starts_with("list")) {
      // TODO: these are hard-coded std::vector <-> numpy array mappings, which is
      // way too simplistic for real use. It only exists for demonstration purposes,
      // until we have an IDL
      auto const dtype = collection_dtype(inp_type);
      if (!dtype) {
        PyErr_Format(PyExc_TypeError, "unsupported collection input type \"%s\"", inp_type.c_str());
        return false;
      }
      if (*dtype == "[int32_t]") {
        insert_converter(mod, pyname, vint_to_py, inp_pq, output, nc);
      } else if (*dtype == "[uint32_t]") {
        insert_converter(mod, pyname, vuint_to_py, inp_pq, output, nc);
      } else if (*dtype == "[int64_t]") {
        insert_converter(mod, pyname, vlong_to_py, inp_pq, output, nc);
      } else if (*dtype == "[uint64_t]") {
        insert_converter(mod, pyname, vulong_to_py, inp_pq, output, nc);
      } else if (*dtype == "[float]") {
        insert_converter(mod, pyname, vfloat_to_py, inp_pq, output, nc);
      } else if (*dtype == "[double]") {
        insert_converter(mod, pyname, vdouble_to_py, inp_pq, output, nc);
      } else {
        PyErr_Format(PyExc_TypeError, "unsupported collection input type \"%s\"", inp_type.c_str());
        return false;
      }
    } else {
      PyErr_Format(PyExc_TypeError, "unsupported input type \"%s\"", inp_type.c_str());
      return false;
    }
  }

  return true;
}

static bool insert_output_converter(py_phlex_module* mod,
                                    std::string const& cname,
                                    product_selector const& out_pq,
                                    std::string const& out_type,
                                    std::string const& output,
                                    bool ispy,
                                    concurrency nc)
{
  // insert output converter node into the graph
  if (out_type == "bool") {
    insert_converter(mod, cname, ispy ? py_to_bool : dcarg_to_bool, out_pq, output, nc);
  } else if (out_type == "int32_t") {
    insert_converter(mod, cname, ispy ? py_to_int : dcarg_to_int, out_pq, output, nc);
  } else if (out_type == "uint32_t") {
    insert_converter(mod, cname, ispy ? py_to_uint : dcarg_to_uint, out_pq, output, nc);
  } else if (out_type == "int64_t") {
    insert_converter(mod, cname, ispy ? py_to_long : dcarg_to_long, out_pq, output, nc);
  } else if (out_type == "uint64_t") {
    insert_converter(mod, cname, ispy ? py_to_ulong : dcarg_to_ulong, out_pq, output, nc);
  } else if (out_type == "float") {
    insert_converter(mod, cname, ispy ? py_to_float : dcarg_to_float, out_pq, output, nc);
  } else if (out_type == "double") {
    insert_converter(mod, cname, ispy ? py_to_double : dcarg_to_double, out_pq, output, nc);
  } else if (out_type.starts_with("ndarray") || out_type.starts_with("list")) {
    // TODO: just like for input types, these are hard-coded, but should be handled by
    // an IDL instead.
    auto const dtype = collection_dtype(out_type);
    if (!dtype) {
      PyErr_Format(PyExc_TypeError, "unsupported collection output type \"%s\"", out_type.c_str());
      return false;
    }
    if (*dtype == "[int32_t]") {
      insert_converter(mod, cname, py_to_vint, out_pq, output, nc);
    } else if (*dtype == "[uint32_t]") {
      insert_converter(mod, cname, py_to_vuint, out_pq, output, nc);
    } else if (*dtype == "[int64_t]") {
      insert_converter(mod, cname, py_to_vlong, out_pq, output, nc);
    } else if (*dtype == "[uint64_t]") {
      insert_converter(mod, cname, py_to_vulong, out_pq, output, nc);
    } else if (*dtype == "[float]") {
      insert_converter(mod, cname, py_to_vfloat, out_pq, output, nc);
    } else if (*dtype == "[double]") {
      insert_converter(mod, cname, py_to_vdouble, out_pq, output, nc);
    } else {
      PyErr_Format(PyExc_TypeError, "unsupported collection output type \"%s\"", out_type.c_str());
      return false;
    }
  } else {
    PyErr_Format(PyExc_TypeError, "unsupported output type \"%s\"", out_type.c_str());
    return false;
  }

  return true;
}

static void* numba_function_address(PyObject* callable)
{
  // Detect Numba and extract its C function pointer; otherwise use the Python dispatcher.
  if (!is_numba_cfunc(callable)) {
    return nullptr;
  }

  PyObject* pyaddr = PyObject_GetAttrString(callable, "address");
  if (!pyaddr) {
    PyErr_Clear();
    return nullptr;
  }
  void* result = PyLong_AsVoidPtr(pyaddr);
  Py_DECREF(pyaddr);
  if (!result) {
    PyErr_Clear();
  }
  return result;
}

static std::optional<identifier> transform_output_layer(
  std::string const& name, std::vector<product_selector> const& input_selectors)
{
  // TODO: output layers are ambiguous when inputs span layers. Reject that case until a
  // well-defined output-layer policy exists.
  auto result = static_cast<identifier>(input_selectors[0].layer);
  for (auto const& selector : input_selectors | std::views::drop(1)) {
    if (static_cast<identifier>(selector.layer) != result) {
      PyErr_Format(PyExc_ValueError, "transform %s output layer is ambiguous", name.c_str());
      return std::nullopt;
    }
  }
  return result;
}

static bool validate_transform_output(std::string const& name,
                                      std::vector<std::string> const& output_types,
                                      std::vector<std::string> const& output_suffixes)
{
  if (output_types.empty()) {
    PyErr_Format(PyExc_TypeError, "transform %s should have an output type", name.c_str());
    return false;
  }
  if (output_suffixes.empty()) {
    PyErr_Format(PyExc_TypeError, "transform %s should have an output suffix", name.c_str());
    return false;
  }
  return true;
}

static bool validate_numba_transform_types(std::string const& name,
                                           std::vector<std::string> const& input_types,
                                           std::string const& output_type,
                                           void* ccallf)
{
  if (!ccallf) {
    return true;
  }

  // TODO: remove this temporary restriction once Numba vector support is available.
  // LCOV_EXCL_START
  auto const is_collection_type = [](std::string const& type) {
    return type.starts_with("ndarray") || type.starts_with("list");
  };
  for (auto const& input_type : input_types) {
    if (is_collection_type(input_type)) {
      PyErr_Format(PyExc_TypeError,
                   "Numba transform %s has unsupported collection input type \"%s\"",
                   name.c_str(),
                   input_type.c_str());
      return false;
    }
  }
  if (is_collection_type(output_type)) {
    PyErr_Format(PyExc_TypeError,
                 "Numba transform %s has unsupported collection output type \"%s\"",
                 name.c_str(),
                 output_type.c_str());
    return false;
  }
  // LCOV_EXCL_STOP
  return true;
}

static product_selector converted_input_selector(std::string const& name,
                                                 std::size_t index,
                                                 product_selector const& selector)
{
  std::string const suffix =
    "py_" + (selector.suffix ? std::string{static_cast<std::string_view>(*selector.suffix)} : "");
  return {.creator = identifier(input_converter_name(name, index)),
          .layer = selector.layer,
          .suffix = identifier(suffix)};
}

template <size_t N, typename Cf>
static bool unroll_switch(size_t rt_size, Cf&& func)
{
  return [&]<size_t... Is>(std::index_sequence<Is...>) {
    // 1-based sequence (all computational nodes have an input, or they can't be scheduled),
    // with the fold expression short-circuited using ||

    // clang-tidy is incorrect here, b/c the condition "rt_size == (Is + 1)" is only ever
    // true once, so the forward is only called once, and func is never used after move
    // NOLINTBEGIN(bugprone-use-after-move)
    bool matched = (... || ((rt_size == (Is + 1))
                              ? (std::forward<Cf>(func)(std::make_index_sequence<Is + 1>{}), true)
                              : false));
    // NOLINTEND(bugprone-use-after-move)

    return matched;
  }(std::make_index_sequence<N>{});
}

static bool register_transform_callback(py_phlex_module* mod,
                                        PyObject* callable,
                                        void* ccallf,
                                        std::string const& name,
                                        std::vector<product_selector> const& input_selectors,
                                        std::string const& output_type,
                                        std::string const& output_suffix,
                                        concurrency nconcur)
{
  // Only a single output is supported until typed tuple conversion is implemented.
  std::string const pyname = "py_" + name;
  std::string const pyoutput = output_suffix + "_py";
  auto register_n_args = [&]<std::size_t... is>(std::index_sequence<is...>) {
    constexpr std::size_t n = sizeof...(is);
    if (ccallf) {
      jit_callback<dcarg, n> callback{callable, ccallf, output_type};
      mod->ph_module->transform(pyname, callback, nconcur)
        .input_family(converted_input_selector(name, is, input_selectors[is])...)
        .output_product_suffixes(pyoutput);
    } else {
      py_callback<dcarg, n> callback{callable};
      mod->ph_module->transform(pyname, callback, nconcur)
        .input_family(converted_input_selector(name, is, input_selectors[is])...)
        .output_product_suffixes(pyoutput);
    }
  };
  if (unroll_switch<max_supported_args>(input_selectors.size(), register_n_args)) {
    return true;
  }
  PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
  return false;
}

static PyObject* md_transform(py_phlex_module* mod, PyObject* args, PyObject* kwds)
{
  // Register a python algorithm by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  std::string cname;
  std::vector<product_selector> input_selectors;
  std::vector<std::string> input_types;
  std::vector<std::string> output_suffixes;
  std::vector<std::string> output_types;
  concurrency nconcur(-1);
  PyObject* callable = parse_args(
    args, kwds, cname, input_selectors, input_types, output_suffixes, output_types, nconcur);

  if (!callable) {
    return nullptr; // error already set
  }

  void* ccallf = numba_function_address(callable);
  if (!validate_transform_output(cname, output_types, output_suffixes)) {
    Py_DECREF(callable);
    return nullptr;
  }

  auto const output_layer = transform_output_layer(cname, input_selectors);
  if (!output_layer) {
    Py_DECREF(callable);
    return nullptr;
  }

  if (!insert_input_converters(mod, cname, input_selectors, input_types, !ccallf, nconcur)) {
    Py_DECREF(callable);
    return nullptr; // error already set
  }

  std::string const& out_type = output_types[0];
  if (!validate_numba_transform_types(cname, input_types, out_type, ccallf) ||
      !register_transform_callback(
        mod, callable, ccallf, cname, input_selectors, out_type, output_suffixes[0], nconcur)) {
    Py_DECREF(callable);
    return nullptr;
  }

  // insert output converter node into the graph
  std::string const pyname = "py_" + cname;
  std::string const pyoutput = output_suffixes[0] + "_py";
  auto out_pq = product_selector{
    .creator = identifier(pyname), .layer = *output_layer, .suffix = identifier(pyoutput)};
  std::string const& output = output_suffixes[0];
  if (!insert_output_converter(mod, cname, out_pq, out_type, output, !ccallf, nconcur)) {
    Py_DECREF(callable);
    return nullptr; // error already set
  }

  Py_DECREF(callable);
  Py_RETURN_NONE;
}

static PyObject* md_observe(py_phlex_module* mod, PyObject* args, PyObject* kwds)
{
  // Register a python observer by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  std::string cname;
  std::vector<product_selector> input_selectors;
  std::vector<std::string> input_types;
  std::vector<std::string> output_suffixes;
  std::vector<std::string> output_types;
  concurrency nconcur(-1);
  PyObject* callable = parse_args(
    args, kwds, cname, input_selectors, input_types, output_suffixes, output_types, nconcur);

  if (!callable) {
    return nullptr; // error already set
  }

  // detect numba and extract C function pointer if any, else use default Python
  // callable dispatcher
  void* ccallf = nullptr;
  if (is_numba_cfunc(callable)) {
    PyObject* pyaddr = PyObject_GetAttrString(callable, "address");
    if (pyaddr) {
      ccallf = PyLong_AsVoidPtr(pyaddr);
      Py_DECREF(pyaddr);
    }
    if (!ccallf) {
      PyErr_Clear();
    }
  }

  if (!output_types.empty()) {
    PyErr_Format(PyExc_TypeError,
                 "an observer should not have an output type (got: \"%s\")",
                 output_types[0].c_str());
    Py_DECREF(callable);
    return nullptr;
  }

  if (!insert_input_converters(mod, cname, input_selectors, input_types, !ccallf, nconcur)) {
    Py_DECREF(callable);
    return nullptr; // error already set
  }

  // register Python observer callbacks
  auto observe_n_args = [&]<size_t... Is>(std::index_sequence<Is...>) {
    constexpr size_t n = sizeof...(Is);

    auto make_product_selector = [&](size_t i) {
      auto pq = input_selectors[i];
      std::string c = input_converter_name(cname, i);
      std::string suff =
        "py_" + (pq.suffix ? std::string{static_cast<std::string_view>(*pq.suffix)} : "");

      return product_selector{
        .creator = identifier(c), .layer = pq.layer, .suffix = identifier(suff)};
    };

    auto insert_observe_for_callback = [&](auto& cb) {
      mod->ph_module->observe(cname, cb, nconcur).input_family(make_product_selector(Is)...);
    };

    if (ccallf) {
      jit_callback<void, n> cb{callable, ccallf, "void"};
      insert_observe_for_callback(cb);
    } else {
      py_callback<void, n> cb{callable};
      insert_observe_for_callback(cb);
    }
  };

  if (!unroll_switch<max_supported_args>(input_selectors.size(), observe_n_args)) {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    Py_DECREF(callable);
    return nullptr;
  }

  Py_DECREF(callable);
  Py_RETURN_NONE;
}

// PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<PyMethodDef, 3> md_methods{
  {{.ml_name = "transform",
    .ml_meth = reinterpret_cast<PyCFunction>(md_transform),
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "register a Python transform"},
   {.ml_name = "observe",
    .ml_meth = reinterpret_cast<PyCFunction>(md_observe),
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "register a Python observer"},
   {.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr}}};

// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::phlex_module_type = {
  // clang-format off
  .ob_base = PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "pyphlex.module",
  // clang-format on
  .tp_basicsize = sizeof(py_phlex_module),
  .tp_itemsize = 0,
  .tp_dealloc = nullptr,
  .tp_vectorcall_offset = 0,
  .tp_getattr = nullptr,
  .tp_setattr = nullptr,
  .tp_as_async = nullptr,
  .tp_repr = nullptr,
  .tp_as_number = nullptr,
  .tp_as_sequence = nullptr,
  .tp_as_mapping = nullptr,
  .tp_hash = nullptr,
  .tp_call = nullptr,
  .tp_str = nullptr,
  .tp_getattro = nullptr,
  .tp_setattro = nullptr,
  .tp_as_buffer = nullptr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "phlex module wrapper",
  .tp_traverse = nullptr,
  .tp_clear = nullptr,
  .tp_richcompare = nullptr,
  .tp_weaklistoffset = 0,
  .tp_iter = nullptr,
  .tp_iternext = nullptr,
  .tp_methods = md_methods.data(),
  .tp_members = nullptr,
  .tp_getset = nullptr,
  .tp_base = nullptr,
  .tp_dict = nullptr,
  .tp_descr_get = nullptr,
  .tp_descr_set = nullptr,
  .tp_dictoffset = 0,
  .tp_init = nullptr,
  .tp_alloc = nullptr,
  .tp_new = nullptr,
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

struct provider_registration {
  PyObject* callable;
  PyObject* output;
  std::string name;
  std::string output_type;
};

// The source wrapper remains here to share the Python converter implementations.
// TODO: move it to a dedicated source-wrapper translation unit.
static std::optional<provider_registration> parse_provider_registration(PyObject* args,
                                                                        PyObject* kwds)
{
  // Python 3.13+ accepts const keyword-name tables. Older versions require mutable strings.
#if PY_VERSION_HEX < 0x030d0000
  // NOLINTBEGIN(modernize-avoid-c-arrays)
  static char kw0[] = "callable";
  static char kw1[] = "output_product";
  static char kw2[] = "name";
  static char* kwnames[] = {kw0, kw1, kw2, nullptr};
  // NOLINTEND(modernize-avoid-c-arrays)
#else
  static std::array<char const*, 4> const kwnames{"callable", "output_product", "name", nullptr};
#endif
  PyObject* callable = nullptr;
  PyObject* output = nullptr;
  PyObject* pyname = nullptr;
  if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "OO|O", std::data(kwnames), &callable, &output, &pyname)) {
    // Error already set by argument parser.
    return std::nullopt;
  }
  if (!callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "given provider is not callable");
    return std::nullopt;
  }
  if (!pyname) {
    pyname = PyObject_GetAttrString(callable, "__name__");
    if (!pyname) {
      // AttributeError already set.
      return std::nullopt;
    }
  } else {
    Py_INCREF(pyname);
  }
  char const* name_text = PyUnicode_AsUTF8(pyname);
  if (!name_text) {
    Py_DECREF(pyname);
    return std::nullopt;
  }
  std::string const name = name_text;
  Py_DECREF(pyname);

  std::vector<std::string> input_types;
  std::vector<std::string> output_types;
  if (!annotations_to_strings(callable, input_types, output_types)) {
    // Python error already set.
    return std::nullopt;
  }
  // A provider takes exactly one data-cell index.
  if (input_types.size() != 1 || input_types[0] != "data_cell_index") {
    PyErr_SetString(PyExc_TypeError, "a provider takes a single \"data_cell_index\" as input");
    return std::nullopt;
  }
  // Providers must produce one non-void output.
  if (output_types.size() != 1 || output_types[0] == "None") {
    PyErr_SetString(PyExc_TypeError, "a provider must have an output");
    return std::nullopt;
  }
  return provider_registration{
    .callable = callable, .output = output, .name = name, .output_type = output_types[0]};
}

static bool register_scalar_provider(py_phlex_source* src,
                                     std::string const& name,
                                     PyObject* callable,
                                     algorithm_name const& creator,
                                     identifier const& suffix,
                                     identifier const& layer,
                                     std::string const& output_type)
{
  if (output_type == "bool") {
    src->ph_source->provide(name, provider_cb_bool{callable})
      .output_product(creator, suffix, layer);
  } else if (output_type == "int32_t") {
    src->ph_source->provide(name, provider_cb_int{callable}).output_product(creator, suffix, layer);
  } else if (output_type == "uint32_t") {
    src->ph_source->provide(name, provider_cb_uint{callable})
      .output_product(creator, suffix, layer);
  } else if (output_type == "int64_t") {
    src->ph_source->provide(name, provider_cb_long{callable})
      .output_product(creator, suffix, layer);
  } else if (output_type == "uint64_t") {
    src->ph_source->provide(name, provider_cb_ulong{callable})
      .output_product(creator, suffix, layer);
  } else if (output_type == "float") {
    src->ph_source->provide(name, provider_cb_float{callable})
      .output_product(creator, suffix, layer);
  } else if (output_type == "double") {
    src->ph_source->provide(name, provider_cb_double{callable})
      .output_product(creator, suffix, layer);
  } else {
    return false;
  }
  return true;
}

static bool register_collection_provider(py_phlex_source* src,
                                         std::string const& name,
                                         PyObject* callable,
                                         algorithm_name const& creator,
                                         identifier const& suffix,
                                         identifier const& layer,
                                         std::string const& output_type)
{
  // TODO: these hard-coded vector mappings should be generated from an IDL.
  auto const dtype = collection_dtype(output_type);
  if (!dtype) {
    PyErr_Format(PyExc_TypeError, "unsupported collection output type \"%s\"", output_type.c_str());
    return false;
  }
  if (*dtype == "[int32_t]") {
    src->ph_source->provide(name, provider_cb_vint{callable})
      .output_product(creator, suffix, layer);
  } else if (*dtype == "[uint32_t]") {
    src->ph_source->provide(name, provider_cb_vuint{callable})
      .output_product(creator, suffix, layer);
  } else if (*dtype == "[int64_t]") {
    src->ph_source->provide(name, provider_cb_vlong{callable})
      .output_product(creator, suffix, layer);
  } else if (*dtype == "[uint64_t]") {
    src->ph_source->provide(name, provider_cb_vulong{callable})
      .output_product(creator, suffix, layer);
  } else if (*dtype == "[float]") {
    src->ph_source->provide(name, provider_cb_vfloat{callable})
      .output_product(creator, suffix, layer);
  } else if (*dtype == "[double]") {
    src->ph_source->provide(name, provider_cb_vdouble{callable})
      .output_product(creator, suffix, layer);
  } else {
    PyErr_Format(PyExc_TypeError, "unsupported collection output type \"%s\"", output_type.c_str());
    return false;
  }
  return true;
}

static PyObject* sc_provide(py_phlex_source* src, PyObject* args, PyObject* kwds)
{
  auto registration = parse_provider_registration(args, kwds);
  if (!registration) {
    return nullptr;
  }
  PyObject* callable = registration->callable;

  // special case of Phlex Variant wrapper
  PyObject* wrapped_callable = PyObject_GetAttrString(callable, "phlex_callable");
  if (wrapped_callable) {
    callable = wrapped_callable;
  } else {
    // no wrapper, use the original callable
    PyErr_Clear();
  }

  // translate and validate the output "selectors"
  // Since a selector in Python is just a dictionary, it isn't called out in the user
  // API as a selector
  auto opq = validate_selector(registration->output);
  if (!opq.has_value()) {
    // validate_selector has set a python exception with details about the error
    Py_XDECREF(wrapped_callable);
    return nullptr;
  }

  algorithm_name creator = algorithm_name::create(std::string_view(*opq.value().creator));
  identifier layer = opq.value().layer;
  identifier suffix = opq.value().suffix.value_or("");

  // Provider callbacks retain the callable for the graph lifetime. A translator node
  // cannot be used here because sources do not have a module for adding one.
  // TODO: callable cleanup is deferred until a Phlex shutdown procedure exists.
  bool const is_collection = registration->output_type.starts_with("ndarray") ||
                             registration->output_type.starts_with("list");
  bool const registered =
    is_collection
      ? register_collection_provider(
          src, registration->name, callable, creator, suffix, layer, registration->output_type)
      : register_scalar_provider(
          src, registration->name, callable, creator, suffix, layer, registration->output_type);
  if (!registered) {
    if (!PyErr_Occurred()) {
      PyErr_Format(
        PyExc_TypeError, "unsupported output type \"%s\"", registration->output_type.c_str());
    }
    Py_XDECREF(wrapped_callable);
    return nullptr;
  }

  Py_XDECREF(wrapped_callable);
  Py_RETURN_NONE;
}

// PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<PyMethodDef, 2> sc_methods{
  {{.ml_name = "provide",
    .ml_meth = reinterpret_cast<PyCFunction>(sc_provide),
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "register a Python provider"},
   {.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr}}};

// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::phlex_source_type = {
  // clang-format off
  .ob_base = PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "pyphlex.source",
  // clang-format on
  .tp_basicsize = sizeof(py_phlex_source),
  .tp_itemsize = 0,
  .tp_dealloc = nullptr,
  .tp_vectorcall_offset = 0,
  .tp_getattr = nullptr,
  .tp_setattr = nullptr,
  .tp_as_async = nullptr,
  .tp_repr = nullptr,
  .tp_as_number = nullptr,
  .tp_as_sequence = nullptr,
  .tp_as_mapping = nullptr,
  .tp_hash = nullptr,
  .tp_call = nullptr,
  .tp_str = nullptr,
  .tp_getattro = nullptr,
  .tp_setattro = nullptr,
  .tp_as_buffer = nullptr,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "phlex source wrapper",
  .tp_traverse = nullptr,
  .tp_clear = nullptr,
  .tp_richcompare = nullptr,
  .tp_weaklistoffset = 0,
  .tp_iter = nullptr,
  .tp_iternext = nullptr,
  .tp_methods = sc_methods.data(),
  .tp_members = nullptr,
  .tp_getset = nullptr,
  .tp_base = nullptr,
  .tp_dict = nullptr,
  .tp_descr_get = nullptr,
  .tp_descr_set = nullptr,
  .tp_dictoffset = 0,
  .tp_init = nullptr,
  .tp_alloc = nullptr,
  .tp_new = nullptr,
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

// NOLINTEND(performance-no-int-to-ptr)

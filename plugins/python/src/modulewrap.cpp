#include "phlex/module.hpp"
#include "wrap.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <vector>

#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL phlex_ARRAY_API
#include <numpy/arrayobject.h>

// Python algorithms are supported by inserting nodes from C++ -> Python,
// followed by the intended call, and another from Python -> C++.
//
// Since product_query inputs, list the creator name, the suffix can remain
// the same through out the chain (as does the layer), distinguishing the
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

using namespace phlex::experimental;
using phlex::concurrency;
using phlex::product_query;

// Simple phlex module wrapper
// clang-format off
struct phlex::experimental::py_phlex_module {
  PyObject_HEAD
  phlex_module_t* ph_module;
};
// clang-format on

PyObject* phlex::experimental::wrap_module(phlex_module_t& module_)
{
  py_phlex_module* pymod = PyObject_New(py_phlex_module, &PhlexModule_Type);
  pymod->ph_module = &module_;

  return (PyObject*)pymod;
}

namespace {

  static inline std::string stringify(std::vector<std::string>& v)
  {
    return fmt::format("{:n}", v);
  }

  static inline std::string stringify(std::vector<product_query>& v)
  {
    return fmt::format("{:n}", std::ranges::views::transform(v, &product_query::to_string));
  }

  static inline std::string input_converter_name(std::string const& algname, size_t arg)
  {
    return fmt::format("{}_arg{}_py", algname, arg);
  }

  static inline PyObject* lifeline_transform(intptr_t arg)
  {
    PyObject* pyobj = (PyObject*)arg;
    if (pyobj && PyObject_TypeCheck(pyobj, &PhlexLifeline_Type)) {
      return ((py_lifeline_t*)pyobj)->m_view;
    }
    return pyobj;
  }

  // callable object managing the callback
  template <size_t N>
  struct py_callback {
    PyObject* m_callable; // owned

    py_callback(PyObject* callable) : m_callable(callable)
    {
      // callable is always non-null here (validated before py_callback construction)
      PyGILRAII gil;
      Py_INCREF(m_callable);
    }
    py_callback(py_callback const& pc) : m_callable(pc.m_callable)
    {
      // Must hold GIL when manipulating reference counts
      PyGILRAII gil;
      Py_INCREF(m_callable);
    }
    py_callback& operator=(py_callback const& pc)
    {
      if (this != &pc) {
        // Must hold GIL when manipulating reference counts
        PyGILRAII gil;
        Py_INCREF(pc.m_callable);
        Py_DECREF(m_callable);
        m_callable = pc.m_callable;
      }
      return *this;
    }
    ~py_callback()
    {
      // TODO: cleanup deferred to Phlex shutdown hook
      // Cannot safely Py_DECREF during arbitrary destruction due to:
      // - TOCTOU race on Py_IsInitialized() without GIL
      // - Module offloading in interpreter cleanup phase 2
    }

    template <typename... Args>
    intptr_t call(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result =
        PyObject_CallFunctionObjArgs(m_callable, lifeline_transform(args)..., nullptr);

      std::string error_msg;
      if (!result) {
        if (!msg_from_py_error(error_msg))
          error_msg = "Unknown python error";
      }

      decref_all(args...);

      if (!error_msg.empty()) {
        throw std::runtime_error(error_msg.c_str());
      }

      return (intptr_t)result;
    }

    template <typename... Args>
    void callv(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result = PyObject_CallFunctionObjArgs(m_callable, (PyObject*)args..., nullptr);

      std::string error_msg;
      if (!result) {
        if (!msg_from_py_error(error_msg))
          error_msg = "Unknown python error";
      } else
        Py_DECREF(result);

      decref_all(args...);

      if (!error_msg.empty()) {
        throw std::runtime_error(error_msg.c_str());
      }
    }

  private:
    template <typename... Args>
    void decref_all(Args... args)
    {
      // helper to decrement reference counts of N arguments
      (Py_DECREF((PyObject*)args), ...);
    }
  };

  // use explicit instatiations to ensure that the function signature can
  // be derived by the graph builder
  struct py_callback_1 : public py_callback<1> {
    intptr_t operator()(intptr_t arg0) { return call(arg0); }
  };

  struct py_callback_2 : public py_callback<2> {
    intptr_t operator()(intptr_t arg0, intptr_t arg1) { return call(arg0, arg1); }
  };

  struct py_callback_3 : public py_callback<3> {
    intptr_t operator()(intptr_t arg0, intptr_t arg1, intptr_t arg2)
    {
      return call(arg0, arg1, arg2);
    }
  };

  struct py_callback_1v : public py_callback<1> {
    void operator()(intptr_t arg0) { callv(arg0); }
  };

  struct py_callback_2v : public py_callback<2> {
    void operator()(intptr_t arg0, intptr_t arg1) { callv(arg0, arg1); }
  };

  struct py_callback_3v : public py_callback<3> {
    void operator()(intptr_t arg0, intptr_t arg1, intptr_t arg2) { callv(arg0, arg1, arg2); }
  };

  static std::vector<product_query> validate_input(PyObject* input)
  {
    std::vector<product_query> cargs;
    if (!input)
      return cargs;

    PyObject* coll = PySequence_Fast(input, "input_family must be a sequence");
    if (!coll)
      return cargs;

    Py_ssize_t len = PySequence_Fast_GET_SIZE(coll);
    cargs.reserve(static_cast<size_t>(len));

    PyObject** items = PySequence_Fast_ITEMS(coll);
    for (Py_ssize_t i = 0; i < len; ++i) {
      PyObject* item = items[i]; // borrowed reference

      if (!PyDict_Check(item)) {
        PyErr_Format(PyExc_TypeError, "input item %d should be a product specifications", (int)i);
        break;
      }

      PyObject* pyc = PyDict_GetItemString(item, "creator");
      if (!pyc || !PyUnicode_Check(pyc)) {
        PyErr_Format(PyExc_ValueError, "missing \"creator\" for input specification");
        break;
      }
      char const* c = PyUnicode_AsUTF8(pyc);

      PyObject* pyl = PyDict_GetItemString(item, "layer");
      if (!pyl || !PyUnicode_Check(pyl)) {
        PyErr_Format(PyExc_ValueError, "missing \"layer\" for input specification");
        break;
      }
      char const* l = PyUnicode_AsUTF8(pyl);

      std::optional<identifier> s;
      PyObject* pys = PyDict_GetItemString(item, "suffix");
      if (pys) {
        if (!PyUnicode_Check(pys)) {
          PyErr_Format(PyExc_TypeError, "provided \"suffix\" is not a string");
          break;
        }
        s = identifier(PyUnicode_AsUTF8(pys));
      } else
        PyErr_Clear();

      cargs.push_back(product_query{.creator = identifier(c), .layer = identifier(l), .suffix = s});
    }

    if (PyErr_Occurred())
      cargs.clear(); // error handled through Python

    return cargs;
  }

  static std::vector<std::string> validate_output(PyObject* output)
  {
    std::vector<std::string> cargs;
    if (!output)
      return cargs;

    PyObject* coll = PySequence_Fast(output, "output_products must be a sequence");
    if (!coll)
      return cargs;

    Py_ssize_t len = PySequence_Fast_GET_SIZE(coll);
    cargs.reserve(static_cast<size_t>(len));

    PyObject** items = PySequence_Fast_ITEMS(coll);
    for (Py_ssize_t i = 0; i < len; ++i) {
      PyObject* item = items[i]; // borrowed reference
      if (!PyUnicode_Check(item)) {
        PyErr_Format(PyExc_TypeError, "item %d must be a string", (int)i);
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

    if (PyErr_Occurred())
      cargs.clear(); // error handled through Python

    return cargs;
  }

} // unnamed namespace

namespace {

  static std::string annotation_as_text(PyObject* pyobj)
  {
    static PyObject* normalizer = nullptr;
    if (!normalizer) {
      PyObject* phlexmod = PyImport_ImportModule("phlex");
      if (phlexmod) {
        normalizer = PyObject_GetAttrString(phlexmod, "normalize_type");
        Py_DECREF(phlexmod);
      }

      if (!normalizer) {
        std::string msg;
        if (msg_from_py_error(msg, false))
          throw std::runtime_error("unable to retrieve the phlex type normalizer: " + msg);
      }
    }

    PyObject* norm = PyObject_CallOneArg(normalizer, pyobj);
    if (!norm) {
      std::string msg;
      if (msg_from_py_error(msg, false))
        throw std::runtime_error("normalization error: " + msg);
    }

    std::string ann = PyUnicode_AsUTF8(norm);
    Py_DECREF(norm);

    return ann;
  }

  // converters of builtin types; TODO: this is a basic subset only, b/c either
  // these will be generated from the IDL, or they should be picked up from an
  // existing place such as cppyy

  static bool pylong_as_bool(PyObject* pyobject)
  {
    // range-checking python integer to C++ bool conversion
    long l = PyLong_AsLong(pyobject);
    // fail to pass float -> bool; the problem is rounding (0.1 -> 0 -> False)
    if (!(l == 0 || l == 1) || PyFloat_Check(pyobject)) {
      PyErr_SetString(PyExc_ValueError, "boolean value should be bool, or integer 1 or 0");
      return (bool)-1;
    }
    return (bool)l;
  }

  static long pylong_as_strictlong(PyObject* pyobject)
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
    return (long)-1;
  }

  static unsigned long pylong_or_int_as_ulong(PyObject* pyobject)
  {
    // convert <pybject> to C++ unsigned long, with bounds checking, allow int -> ulong.
    if (PyFloat_Check(pyobject)) {
      PyErr_SetString(PyExc_TypeError, "can\'t convert float to unsigned long");
      return (unsigned long)-1;
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
    if (ul == (unsigned long)-1 && PyErr_Occurred() && PyLong_Check(pyobject)) {
      PyErr_Clear();
      long i = PyLong_AS_LONG(pyobject);
      if (0 <= i) {
        ul = (unsigned long)i;
      } else {
        PyErr_SetString(PyExc_ValueError, "can\'t convert negative value to unsigned long");
        return (unsigned long)-1;
      }
    }

    return ul;
  }

#define BASIC_CONVERTER(name, cpptype, topy, frompy)                                               \
  static intptr_t name##_to_py(cpptype a)                                                          \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
    return (intptr_t)topy(a);                                                                      \
  }                                                                                                \
                                                                                                   \
  static cpptype py_to_##name(intptr_t pyobj)                                                      \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
    cpptype i = (cpptype)frompy((PyObject*)pyobj);                                                 \
    std::string msg;                                                                               \
    if (msg_from_py_error(msg, true)) {                                                            \
      Py_DECREF((PyObject*)pyobj);                                                                 \
      throw std::runtime_error("Python conversion error for type " #name ": " + msg);              \
    }                                                                                              \
    Py_DECREF((PyObject*)pyobj);                                                                   \
    return i;                                                                                      \
  }

  BASIC_CONVERTER(bool, bool, PyBool_FromLong, pylong_as_bool)
  BASIC_CONVERTER(int, std::int32_t, PyLong_FromLong, PyLong_AsLong)
  BASIC_CONVERTER(uint, std::uint32_t, PyLong_FromLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(long, std::int64_t, PyLong_FromLong, pylong_as_strictlong)
  BASIC_CONVERTER(ulong, std::uint64_t, PyLong_FromUnsignedLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(float, float, PyFloat_FromDouble, PyFloat_AsDouble)
  BASIC_CONVERTER(double, double, PyFloat_FromDouble, PyFloat_AsDouble)

#define VECTOR_CONVERTER(name, cpptype, nptype)                                                    \
  static intptr_t name##_to_py(std::shared_ptr<std::vector<cpptype>> const& v)                     \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
                                                                                                   \
    if (!v)                                                                                        \
      return (intptr_t)nullptr;                                                                    \
                                                                                                   \
    /* use a numpy view with the shared pointer tied up in a lifeline object (note: this */        \
    /* is just a demonstrator; alternatives are still being considered) */                         \
    npy_intp dims[] = {static_cast<npy_intp>(v->size())};                                          \
                                                                                                   \
    PyObject* np_view = PyArray_SimpleNewFromData(1,                 /* 1-D array */               \
                                                  dims,              /* dimension sizes */         \
                                                  nptype,            /* numpy C type */            \
                                                  (void*)(v->data()) /* raw buffer */              \
    );                                                                                             \
                                                                                                   \
    if (!np_view)                                                                                  \
      return (intptr_t)nullptr;                                                                    \
                                                                                                   \
    /* make the data read-only by not making it writable */                                        \
    PyArray_CLEARFLAGS((PyArrayObject*)np_view, NPY_ARRAY_WRITEABLE);                              \
                                                                                                   \
    /* create a lifeline object to tie this array and the original handle together; note */        \
    /* that the callback code needs to pick the data member out of the lifeline object, */         \
    /* when passing it to the registered Python function */                                        \
    py_lifeline_t* pyll =                                                                          \
      (py_lifeline_t*)PhlexLifeline_Type.tp_new(&PhlexLifeline_Type, nullptr, nullptr);            \
    if (!pyll) {                                                                                   \
      Py_DECREF(np_view);                                                                          \
      return (intptr_t)nullptr;                                                                    \
    }                                                                                              \
    pyll->m_source = v;                                                                            \
    pyll->m_view = np_view; /* steals reference */                                                 \
                                                                                                   \
    return (intptr_t)pyll;                                                                         \
  }

  VECTOR_CONVERTER(vint, std::int32_t, NPY_INT32)
  VECTOR_CONVERTER(vuint, std::uint32_t, NPY_UINT32)
  VECTOR_CONVERTER(vlong, std::int64_t, NPY_INT64)
  VECTOR_CONVERTER(vulong, std::uint64_t, NPY_UINT64)
  VECTOR_CONVERTER(vfloat, float, NPY_FLOAT)
  VECTOR_CONVERTER(vdouble, double, NPY_DOUBLE)

#define NUMPY_ARRAY_CONVERTER(name, cpptype, nptype, frompy)                                       \
  static std::shared_ptr<std::vector<cpptype>> py_to_##name(intptr_t pyobj)                        \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
                                                                                                   \
    auto vec = std::make_shared<std::vector<cpptype>>();                                           \
                                                                                                   \
    /* TODO: because of unresolved ownership issues, copy the full array contents */               \
    if (PyArray_Check((PyObject*)pyobj)) {                                                         \
      PyArrayObject* arr = (PyArrayObject*)pyobj;                                                  \
                                                                                                   \
      /* TODO: flattening the array here seems to be the only workable solution */                 \
      npy_intp* dims = PyArray_DIMS(arr);                                                          \
      int nd = PyArray_NDIM(arr);                                                                  \
      size_t total = 1;                                                                            \
      for (int i = 0; i < nd; ++i)                                                                 \
        total *= static_cast<size_t>(dims[i]);                                                     \
                                                                                                   \
      /* copy the array info; note that this assumes C continuity */                               \
      cpptype* raw = static_cast<cpptype*>(PyArray_DATA(arr));                                     \
      vec->reserve(total);                                                                         \
      vec->insert(vec->end(), raw, raw + total);                                                   \
    } else if (PyList_Check((PyObject*)pyobj)) {                                                   \
      Py_ssize_t total = PyList_Size((PyObject*)pyobj);                                            \
      vec->reserve(total);                                                                         \
      for (Py_ssize_t i = 0; i < total; ++i) {                                                     \
        PyObject* item = PyList_GetItem((PyObject*)pyobj, i);                                      \
        vec->push_back((cpptype)frompy(item));                                                     \
        if (PyErr_Occurred()) {                                                                    \
          PyErr_Clear();                                                                           \
          break;                                                                                   \
        }                                                                                          \
      }                                                                                            \
    } else {                                                                                       \
      std::string msg;                                                                             \
      if (msg_from_py_error(msg, true)) {                                                          \
        throw std::runtime_error("List conversion error: " + msg);                                 \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    Py_DECREF((PyObject*)pyobj);                                                                   \
    return vec;                                                                                    \
  }

  NUMPY_ARRAY_CONVERTER(vint, std::int32_t, NPY_INT32, PyLong_AsLong)
  NUMPY_ARRAY_CONVERTER(vuint, std::uint32_t, NPY_UINT32, pylong_or_int_as_ulong)
  NUMPY_ARRAY_CONVERTER(vlong, std::int64_t, NPY_INT64, pylong_as_strictlong)
  NUMPY_ARRAY_CONVERTER(vulong, std::uint64_t, NPY_UINT64, pylong_or_int_as_ulong)
  NUMPY_ARRAY_CONVERTER(vfloat, float, NPY_FLOAT, PyFloat_AsDouble)
  NUMPY_ARRAY_CONVERTER(vdouble, double, NPY_DOUBLE, PyFloat_AsDouble)

  // helpers for inserting converter nodes
  template <typename R, typename... Args>
  void insert_converter(py_phlex_module* mod,
                        std::string const& name,
                        R (*converter)(Args...),
                        product_query pq_in,
                        std::string const& output)
  {
    mod->ph_module->transform(name, converter, concurrency::serial)
      .input_family(pq_in)
      .output_products(output);
  }

} // unnamed namespace

static PyObject* parse_args(PyObject* args,
                            PyObject* kwds,
                            std::string& functor_name,
                            std::vector<product_query>& input_queries,
                            std::vector<std::string>& input_types,
                            std::vector<std::string>& output_labels,
                            std::vector<std::string>& output_types)
{
  // Helper function to extract the common names and identifiers needed to insert
  // any node. (The observer does not require outputs, but they still need to be
  // retrieved, not ignored, to issue an error message if an output is provided.)

  static char const* kwnames[] = {
    "callable", "input_family", "output_products", "concurrency", "name", nullptr};
  PyObject *callable = 0, *input = 0, *output = 0, *concurrency = 0, *pyname = 0;
  if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "OO|OOO", (char**)kwnames, &callable, &input, &output, &concurrency, &pyname)) {
    // error already set by argument parser
    return nullptr;
  }

  if (concurrency && concurrency != Py_None) {
    PyErr_SetString(PyExc_TypeError, "only serial concurrency is supported");
    return nullptr;
  }

  if (!callable || !PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "provided algorithm is not callable");
    return nullptr;
  }

  // retrieve function name and argument types
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
  input_queries = validate_input(input);
  if (input_queries.empty()) {
    if (!PyErr_Occurred()) {
      PyErr_Format(PyExc_ValueError,
                   "no input provided for %s; node can not be scheduled",
                   functor_name.c_str());
    }
    return nullptr;
  }

  // convert output declarations, to be able to pass them to Phlex
  output_labels = validate_output(output);
  if (output_labels.size() > 1) {
    PyErr_SetString(PyExc_TypeError, "only a single output supported");
    return nullptr;
  }

  // retrieve C++ (matching) types from annotations
  input_types.reserve(input_queries.size());

  PyObject* sann = PyUnicode_FromString("__annotations__");
  PyObject* annot = PyObject_GetAttr(callable, sann);
  if (!annot) {
    // the callable may be an instance with a __call__ method
    PyErr_Clear();
    PyObject* callm = PyObject_GetAttrString(callable, "__call__");
    if (callm) {
      annot = PyObject_GetAttr(callm, sann);
      Py_DECREF(callm);
    }
  }
  Py_DECREF(sann);

  if (annot && PyDict_Check(annot)) {
    // Variant guarantees OrderedDict with "return" last
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(annot, &pos, &key, &value)) {
      char const* key_str = PyUnicode_AsUTF8(key);
      if (strcmp(key_str, "return") == 0) {
        output_types.push_back(annotation_as_text(value));
      } else {
        input_types.push_back(annotation_as_text(value));
      }
    }
  }
  Py_XDECREF(annot);

  // ignore None as Python's conventional "void" return, which is meaningless in C++
  if (output_types.size() == 1 && output_types[0] == "None")
    output_types.clear();

  // if annotations were correct (and correctly parsed), there should be as many
  // input types as input labels
  if (input_types.size() != input_queries.size()) {
    PyErr_Format(PyExc_TypeError,
                 "number of inputs (%d; %s) does not match number of annotation types (%d; %s)",
                 input_queries.size(),
                 stringify(input_queries).c_str(),
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

static bool insert_input_converters(py_phlex_module* mod,
                                    std::string const& cname, // TODO: shared_ptr<PyObject>
                                    std::vector<product_query> const& input_queries,
                                    std::vector<std::string> const& input_types)
{
  // insert input converter nodes into the graph
  for (size_t i = 0; i < (size_t)input_queries.size(); ++i) {
    // TODO: this seems overly verbose and inefficient, but the function needs
    // to be properly types, so every option is made explicit
    auto const& inp_pq = input_queries[i];
    auto const& inp_type = input_types[i];

    std::string const& pyname = input_converter_name(cname, i);
    std::string output =
      "py_" + (inp_pq.suffix ? std::string{static_cast<std::string_view>(*inp_pq.suffix)} : "");

    if (inp_type == "bool")
      insert_converter(mod, pyname, bool_to_py, inp_pq, output);
    else if (inp_type == "int32_t")
      insert_converter(mod, pyname, int_to_py, inp_pq, output);
    else if (inp_type == "uint32_t")
      insert_converter(mod, pyname, uint_to_py, inp_pq, output);
    else if (inp_type == "int64_t")
      insert_converter(mod, pyname, long_to_py, inp_pq, output);
    else if (inp_type == "uint64_t")
      insert_converter(mod, pyname, ulong_to_py, inp_pq, output);
    else if (inp_type == "float")
      insert_converter(mod, pyname, float_to_py, inp_pq, output);
    else if (inp_type == "double")
      insert_converter(mod, pyname, double_to_py, inp_pq, output);
    else if (inp_type.compare(0, 7, "ndarray") == 0 || inp_type.compare(0, 4, "list") == 0) {
      // TODO: these are hard-coded std::vector <-> numpy array mappings, which is
      // way too simplistic for real use. It only exists for demonstration purposes,
      // until we have an IDL
      std::string_view dtype{inp_type.begin() + inp_type.rfind('['), inp_type.end()};
      if (dtype == "[int32_t]") {
        insert_converter(mod, pyname, vint_to_py, inp_pq, output);
      } else if (dtype == "[uint32_t]") {
        insert_converter(mod, pyname, vuint_to_py, inp_pq, output);
      } else if (dtype == "[int64_t]") {
        insert_converter(mod, pyname, vlong_to_py, inp_pq, output);
      } else if (dtype == "[uint64_t]") {
        insert_converter(mod, pyname, vulong_to_py, inp_pq, output);
      } else if (dtype == "[float]") {
        insert_converter(mod, pyname, vfloat_to_py, inp_pq, output);
      } else if (dtype == "[double]") {
        insert_converter(mod, pyname, vdouble_to_py, inp_pq, output);
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

static PyObject* md_transform(py_phlex_module* mod, PyObject* args, PyObject* kwds)
{
  // Register a python algorithm by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  std::string cname;
  std::vector<product_query> input_queries;
  std::vector<std::string> input_types, output_labels, output_types;
  PyObject* callable =
    parse_args(args, kwds, cname, input_queries, input_types, output_labels, output_types);
  if (!callable)
    return nullptr; // error already set

  if (output_types.empty()) {
    PyErr_Format(PyExc_TypeError, "transform %s should have an output type", cname.c_str());
    Py_DECREF(callable);
    return nullptr;
  }

  // TODO: it's not clear what the output layer will be if the input layers are not
  // all the same, so for now, simply raise an error if their is any ambiguity
  auto output_layer = static_cast<identifier>(input_queries[0].layer);
  if (1 < input_queries.size()) {
    for (std::vector<product_query>::size_type iq = 1; iq < input_queries.size(); ++iq) {
      if (static_cast<identifier>(input_queries[iq].layer) != output_layer) {
        PyErr_Format(PyExc_ValueError, "transform %s output layer is ambiguous", cname.c_str());
        Py_DECREF(callable);
        return nullptr;
      }
    }
  }

  if (!insert_input_converters(mod, cname, input_queries, input_types)) {
    Py_DECREF(callable);
    return nullptr; // error already set
  }

  // register Python transform

  // TODO: only support single output type for now, as there has to be a mapping
  // onto a std::tuple otherwise, which is a typed object, thus complicating the
  // template instantiation
  std::string pyname = "py_" + cname;
  std::string pyoutput = output_labels[0] + "_py";

  auto pq0 = input_queries[0];
  std::string c0 = input_converter_name(cname, 0);
  std::string suff0 =
    "py_" + (pq0.suffix ? std::string{static_cast<std::string_view>(*pq0.suffix)} : "");

  switch (input_queries.size()) {
  case 1: {
    auto* pyc = new py_callback_1{callable}; // TODO: leaks, but has program lifetime
    mod->ph_module->transform(pyname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)})
      .output_products(pyoutput);
    break;
  }
  case 2: {
    auto* pyc = new py_callback_2{callable};
    auto pq1 = input_queries[1];
    std::string c1 = input_converter_name(cname, 1);
    std::string suff1 =
      "py_" + (pq1.suffix ? std::string{static_cast<std::string_view>(*pq1.suffix)} : "");
    mod->ph_module->transform(pyname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)},
        product_query{.creator = identifier(c1), .layer = pq1.layer, .suffix = identifier(suff1)})
      .output_products(pyoutput);
    break;
  }
  case 3: {
    auto* pyc = new py_callback_3{callable};
    auto pq1 = input_queries[1];
    std::string c1 = input_converter_name(cname, 1);
    std::string suff1 =
      "py_" + (pq1.suffix ? std::string{static_cast<std::string_view>(*pq1.suffix)} : "");
    auto pq2 = input_queries[2];
    std::string c2 = input_converter_name(cname, 2);
    std::string suff2 =
      "py_" + (pq2.suffix ? std::string{static_cast<std::string_view>(*pq2.suffix)} : "");
    mod->ph_module->transform(pyname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)},
        product_query{.creator = identifier(c1), .layer = pq1.layer, .suffix = identifier(suff1)},
        product_query{.creator = identifier(c2), .layer = pq2.layer, .suffix = identifier(suff2)})
      .output_products(pyoutput);
    break;
  }
  default: {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    Py_DECREF(callable);
    return nullptr;
  }
  }

  // insert output converter node into the graph
  auto out_pq = product_query{.creator = identifier(pyname),
                              .layer = identifier(output_layer),
                              .suffix = identifier(pyoutput)};
  std::string out_type = output_types[0];
  std::string output = output_labels[0];
  if (out_type == "bool")
    insert_converter(mod, cname, py_to_bool, out_pq, output);
  else if (out_type == "int32_t")
    insert_converter(mod, cname, py_to_int, out_pq, output);
  else if (out_type == "uint32_t")
    insert_converter(mod, cname, py_to_uint, out_pq, output);
  else if (out_type == "int64_t")
    insert_converter(mod, cname, py_to_long, out_pq, output);
  else if (out_type == "uint64_t")
    insert_converter(mod, cname, py_to_ulong, out_pq, output);
  else if (out_type == "float")
    insert_converter(mod, cname, py_to_float, out_pq, output);
  else if (out_type == "double")
    insert_converter(mod, cname, py_to_double, out_pq, output);
  else if (out_type.compare(0, 7, "ndarray") == 0 || out_type.compare(0, 4, "list") == 0) {
    // TODO: just like for input types, these are hard-coded, but should be handled by
    // an IDL instead.
    std::string_view dtype{out_type.begin() + out_type.rfind('['), out_type.end()};
    if (dtype == "[int32_t]") {
      insert_converter(mod, cname, py_to_vint, out_pq, output);
    } else if (dtype == "[uint32_t]") {
      insert_converter(mod, cname, py_to_vuint, out_pq, output);
    } else if (dtype == "[int64_t]") {
      insert_converter(mod, cname, py_to_vlong, out_pq, output);
    } else if (dtype == "[uint64_t]") {
      insert_converter(mod, cname, py_to_vulong, out_pq, output);
    } else if (dtype == "[float]") {
      insert_converter(mod, cname, py_to_vfloat, out_pq, output);
    } else if (dtype == "[double]") {
      insert_converter(mod, cname, py_to_vdouble, out_pq, output);
    } else {
      PyErr_Format(PyExc_TypeError, "unsupported collection output type \"%s\"", out_type.c_str());
      return nullptr;
    }
  } else {
    PyErr_Format(PyExc_TypeError, "unsupported output type \"%s\"", out_type.c_str());
    return nullptr;
  }

  Py_RETURN_NONE;
}

static PyObject* md_observe(py_phlex_module* mod, PyObject* args, PyObject* kwds)
{
  // Register a python observer by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  std::string cname;
  std::vector<product_query> input_queries;
  std::vector<std::string> input_types, output_labels, output_types;
  PyObject* callable =
    parse_args(args, kwds, cname, input_queries, input_types, output_labels, output_types);
  if (!callable)
    return nullptr; // error already set

  if (!output_types.empty()) {
    PyErr_Format(PyExc_TypeError, "an observer should not have an output type");
    return nullptr;
  }

  if (!insert_input_converters(mod, cname, input_queries, input_types)) {
    Py_DECREF(callable);
    return nullptr; // error already set
  }

  // register Python observer
  auto pq0 = input_queries[0];
  std::string c0 = input_converter_name(cname, 0);
  std::string suff0 =
    "py_" + (pq0.suffix ? std::string{static_cast<std::string_view>(*pq0.suffix)} : "");

  switch (input_queries.size()) {
  case 1: {
    auto* pyc = new py_callback_1v{callable};
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)});
    break;
  }
  case 2: {
    auto* pyc = new py_callback_2v{callable};
    auto pq1 = input_queries[1];
    std::string c1 = input_converter_name(cname, 1);
    std::string suff1 =
      "py_" + (pq1.suffix ? std::string{static_cast<std::string_view>(*pq1.suffix)} : "");
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)},
        product_query{.creator = identifier(c1), .layer = pq1.layer, .suffix = identifier(suff1)});
    break;
  }
  case 3: {
    auto* pyc = new py_callback_3v{callable};
    auto pq1 = input_queries[1];
    std::string c1 = input_converter_name(cname, 1);
    std::string suff1 =
      "py_" + (pq1.suffix ? std::string{static_cast<std::string_view>(*pq1.suffix)} : "");
    auto pq2 = input_queries[2];
    std::string c2 = input_converter_name(cname, 2);
    std::string suff2 =
      "py_" + (pq2.suffix ? std::string{static_cast<std::string_view>(*pq2.suffix)} : "");
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{.creator = identifier(c0), .layer = pq0.layer, .suffix = identifier(suff0)},
        product_query{.creator = identifier(c1), .layer = pq1.layer, .suffix = identifier(suff1)},
        product_query{.creator = identifier(c2), .layer = pq2.layer, .suffix = identifier(suff2)});
    break;
  }
  default: {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    Py_DECREF(callable);
    return nullptr;
  }
  }

  Py_RETURN_NONE;
}

static PyMethodDef md_methods[] = {{(char*)"transform",
                                    (PyCFunction)md_transform,
                                    METH_VARARGS | METH_KEYWORDS,
                                    (char*)"register a Python transform"},
                                   {(char*)"observe",
                                    (PyCFunction)md_observe,
                                    METH_VARARGS | METH_KEYWORDS,
                                    (char*)"register a Python observer"},
                                   {(char*)nullptr, nullptr, 0, nullptr}};

// clang-format off
PyTypeObject phlex::experimental::PhlexModule_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  (char*)"pyphlex.module",       // tp_name
  sizeof(py_phlex_module),       // tp_basicsize
  0,                             // tp_itemsize
  0,                             // tp_dealloc
  0,                             // tp_vectorcall_offset / tp_print
  0,                             // tp_getattr
  0,                             // tp_setattr
  0,                             // tp_as_async / tp_compare
  0,                             // tp_repr
  0,                             // tp_as_number
  0,                             // tp_as_sequence
  0,                             // tp_as_mapping
  0,                             // tp_hash
  0,                             // tp_call
  0,                             // tp_str
  0,                             // tp_getattro
  0,                             // tp_setattro
  0,                             // tp_as_buffer
  Py_TPFLAGS_DEFAULT,            // tp_flags
  (char*)"phlex module wrapper", // tp_doc
  0,                             // tp_traverse
  0,                             // tp_clear
  0,                             // tp_richcompare
  0,                             // tp_weaklistoffset
  0,                             // tp_iter
  0,                             // tp_iternext
  md_methods,                    // tp_methods
  0,                             // tp_members
  0,                             // tp_getset
  0,                             // tp_base
  0,                             // tp_dict
  0,                             // tp_descr_get
  0,                             // tp_descr_set
  0,                             // tp_dictoffset
  0,                             // tp_init
  0,                             // tp_alloc
  0,                             // tp_new
  0,                             // tp_free
  0,                             // tp_is_gc
  0,                             // tp_bases
  0,                             // tp_mro
  0,                             // tp_cache
  0,                             // tp_subclasses
  0                              // tp_weaklist
#if PY_VERSION_HEX >= 0x02030000
  , 0                            // tp_del
#endif
#if PY_VERSION_HEX >= 0x02060000
  , 0                            // tp_version_tag
#endif
#if PY_VERSION_HEX >= 0x03040000
  , 0                            // tp_finalize
#endif
#if PY_VERSION_HEX >= 0x03080000
  , 0                            // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
  , 0                            // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
  , 0                            // tp_versions_used
#endif
};
// clang-format on

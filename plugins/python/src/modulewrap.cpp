#include "dyncall.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "wrap.hpp"

#include <fmt/format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
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

  // small helpers to refactor common code
  inline bool is_collection_type(std::string_view type)
  {
    return type.starts_with("ndarray") || type.starts_with("list");
  }

  inline std::string stringify(std::vector<std::string> const& v) { return fmt::format("{:n}", v); }

  inline std::string stringify(std::vector<product_selector> const& v)
  {
    return fmt::format("{:n}", std::ranges::views::transform(v, &product_selector::to_string));
  }

  inline std::string input_converter_name(std::string const& algname, size_t arg)
  {
    return fmt::format("{}_arg{}_py", algname, arg);
  }

  inline std::string converted_suffix(product_selector const& sel)
  {
    return "py_" + (sel.suffix ? std::string{static_cast<std::string_view>(*sel.suffix)} : "");
  }

  inline PyObject* import_attr(char const* module, char const* attr)
  {
    // returns new reference or nullptr with a Python error set
    py_owned const mod{PyImport_ImportModule(module)};
    return mod ? PyObject_GetAttrString(mod.get(), attr) : nullptr;
  }

  PyObject* unwrap_variant(PyObject* callable)
  {
    if (PyObject* wrapped = PyObject_GetAttrString(callable, "phlex_callable")) {
      // PyObject_GetAttrString returns a new reference, which we return
      return wrapped;
    }
    // No wrapper, use the original callable with incremented reference count
    PyErr_Clear();
    Py_INCREF(callable);
    return callable;
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
      py_gilraii const gil;
      Py_INCREF(callable);
    }
    py_callback_base(py_callback_base const& pc) : callable(pc.callable), ccallback(pc.ccallback)
    {
      // Must hold GIL when manipulating reference counts
      py_gilraii const gil;
      Py_INCREF(callable);
    }
    py_callback_base& operator=(py_callback_base const& pc)
    {
      if (this != &pc) {
        py_gilraii const gil;
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

      py_gilraii const gil;

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
  inline std::optional<identifier> try_item(PyObject* pysel,
                                            std::string const& item,
                                            bool allow_optionals)
  {
    PyObject* pyii = PyDict_GetItemString(pysel, item.c_str());
    if ((!pyii && !allow_optionals) || (pyii && !PyUnicode_Check(pyii))) {
      PyErr_Format(PyExc_TypeError, ("missing " + item + " or not a string").c_str());
      return std::nullopt;
    }
    if (!pyii) {
      PyErr_Clear();
      return std::nullopt;
    }
    return std::optional<identifier>{PyUnicode_AsUTF8(pyii)};
  }

  inline std::optional<product_selector> validate_selector(PyObject* pysel,
                                                           bool allow_optionals = true)
  {
    if (!PyDict_Check(pysel)) {
      PyErr_Format(PyExc_TypeError, "selector should be a product specification");
      return std::nullopt;
    }

    std::optional<identifier> c = try_item(pysel, "creator", allow_optionals);
    std::optional<identifier> l = try_item(pysel, "layer", allow_optionals);
    if (!allow_optionals && !(c.has_value() && l.has_value())) {
      return std::nullopt;
    }
    std::optional<identifier> const s = try_item(pysel, "suffix", true); // always optional
    if (!s.has_value() && PyErr_Occurred()) {
      return std::nullopt;
    }

    // in the following, each of these parameters is passed differently b/c:
    //   "c" and "layer" are internal types that only takes an optional through a
    //     move for its contructor
    //   "suffix" is an optional itself, so can pass directly
    return std::optional<product_selector>{
      product_selector{.creator = std::move(c), .layer = std::move(l), .suffix = s}};
  }

  template <typename T, typename F>
  std::vector<T> collect_sequence(PyObject* seq, char const* type_error, F convert)
  {
    std::vector<T> result;
    if (!seq) {
      return result;
    }

    py_owned const coll{PySequence_Fast(seq, type_error)};
    if (!coll) {
      return result;
    }

    Py_ssize_t const len = PySequence_Fast_GET_SIZE(coll.get());
    result.reserve(static_cast<size_t>(len));
    PyObject* const* items = PySequence_Fast_ITEMS(coll.get());
    for (Py_ssize_t i = 0; i < len; ++i) {
      std::optional<T> value = convert(items[i], i);
      if (!value) {
        break; // converter has set a Python error
      }
      result.push_back(std::move(*value));
    }

    if (PyErr_Occurred()) {
      result.clear();
    }
    return result;
  }

  std::vector<product_selector> validate_input(PyObject* input)
  {
    return collect_sequence<product_selector>(
      input, "input_family must be a sequence", [](PyObject* item, Py_ssize_t) {
        return validate_selector(item);
      });
  }

  std::vector<std::string> validate_output(PyObject* output)
  {
    return collect_sequence<std::string>(
      output,
      "output_product_suffixes must be a sequence",
      [](PyObject* item, Py_ssize_t i) -> std::optional<std::string> {
        if (!PyUnicode_Check(item)) {
          PyErr_Format(PyExc_TypeError, "item %d must be a string", static_cast<int>(i));
          return std::nullopt;
        }
        Py_ssize_t sz = -1;
        char const* p = PyUnicode_AsUTF8AndSize(item, &sz);
        if (!p) {
          return std::nullopt;
        }
        return std::string(p, static_cast<std::string::size_type>(sz));
      });
  }

} // unnamed namespace

namespace {

  bool is_numba_cfunc(PyObject* obj)
  {
    static PyObject* cfunc_type = nullptr;
    static bool checked = false;
    if (!checked) {
      checked = true;

      cfunc_type = import_attr("numba.core.ccallback", "CFunc");
      if (!cfunc_type) {
        PyErr_Clear();
      }
      // hard reference to cfunc_type here if not null
    }

    if (!cfunc_type) {
      return false;
    }

    int const result = PyObject_IsInstance(obj, cfunc_type);
    return result == 1;
  }

  std::string annotation_as_text(PyObject* pyobj)
  {
    static PyObject* normalizer = nullptr;

    // a return buffer is needed because the normalizer result is a Python
    // object that needs to be dereferenced before this function returns
    std::string result;

    if (!normalizer) {
      normalizer = import_attr("phlex", "normalize_type");

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
  // NOLINTNEXTLINE(readability-function-size)
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
          std::string const& ret_ann = annotation_as_text(ret);
          if (!ret_ann.empty()) {
            output_types.push_back(ret_ann);
            for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(args); ++i) {
              PyObject* item = PyTuple_GET_ITEM(args, i);
              std::string const& inp_ann = annotation_as_text(item);
              if (inp_ann.empty()) {
                break;
              }
              input_types.push_back(inp_ann);
            }

            if (static_cast<Py_ssize_t>(input_types.size()) == PyTuple_GET_SIZE(args)) {
              conversion_ok = true;
            }
          }
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
    long const l = PyLong_AsLong(pyobject);
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
      long const result = PyLong_AsLong(pylong);
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
      unsigned long const result = PyLong_AsUnsignedLong(pylong);
      Py_DECREF(pylong);
      return result;
    }

    unsigned long ul = PyLong_AsUnsignedLong(pyobject);
    // In this case, using 'std::cmp_equal(ul, -1)' returns the wrong result, so
    // we suppress the clang-tidy warning re. 'ul == static_cast<unsigned long>(-1)'.
    // NOLINTNEXTLINE(modernize-use-integer-sign-comparison)
    if (ul == static_cast<unsigned long>(-1) && PyErr_Occurred() && PyLong_Check(pyobject)) {
      PyErr_Clear();
      long const i = PyLong_AS_LONG(pyobject);
      if (0 <= i) {
        ul = static_cast<unsigned long>(i);
      } else {
        PyErr_SetString(PyExc_ValueError, "can\'t convert negative value to unsigned long");
        return static_cast<unsigned long>(-1);
      }
    }

    return ul;
  }

  // workaround to allow const char* use as a template argument; may eventually
  // end up in the standard, after which it can be dropped ...
  template <std::size_t N>
  struct fixed_string {
    std::array<char, N> buf{};
    // NOLINTNEXTLINE(modernize-avoid-c-arrays): must bind string literals for NTTP deduction
    constexpr fixed_string(char const (&s)[N]) { std::copy_n(s, N, buf.begin()); }
    constexpr std::string_view view() const { return {buf.data(), N - 1}; }
  };

  template <typename T, fixed_string Name, fixed_string Annotation, auto ToPy, auto FromPy>
  struct scalar_converter {
    static constexpr std::string_view key = Annotation.view();

    static dcarg to_py(T a)
    {
      py_gilraii const gil;
      return dcarg{ToPy(a)};
    }

    static dcarg to_dcarg(T a) { return dcarg{a}; }

    static T from_py(dcarg a)
    {
      py_gilraii const gil;
      PyObject* pyobj = a.get<PyObject*>();
      auto i = static_cast<T>(FromPy(pyobj));
      std::string msg;
      if (msg_from_py_error(msg, true)) {
        Py_DECREF(pyobj);
        throw std::runtime_error("Python conversion error for type " + std::string{Name.view()} +
                                 ": " + msg);
      }
      Py_DECREF(pyobj);
      return i;
    }

    static T from_dcarg(dcarg a) { return a.get<T>(); }

    struct provider : py_callback<dcarg, 1> {
      using base = py_callback<dcarg, 1>;
      using base::base;

      T operator()(data_cell_index const& id)
      {
        py_gilraii const gil;
        PyObject* arg0 = wrap_dci(id);
        dcarg const res = base::operator()(dcarg{arg0}); // decrefs arg0
        PyObject* pyres = res.get<PyObject*>();
        T const cres = FromPy(pyres);
        std::string msg;
        if (msg_from_py_error(msg, true)) {
          Py_DECREF(pyres);
          throw std::runtime_error("Python provider conversion error for type " +
                                   std::string{Name.view()} + ": " + msg);
        }
        Py_DECREF(pyres);
        return cres;
      }
    };
  };

  // prefer to retain the columnar layout even if that leads to too long lines
  // clang-format off
  using bool_cnv   = scalar_converter<bool,          "bool",   "bool",     PyBool_FromLong,         pylong_as_bool>;
  using int_cnv    = scalar_converter<std::int32_t,  "int",    "int32_t",  PyLong_FromLong,         PyLong_AsLong>;
  using uint_cnv   = scalar_converter<std::uint32_t, "uint",   "uint32_t", PyLong_FromLong,         pylong_or_int_as_ulong>;
  using long_cnv   = scalar_converter<ph_long_t,     "long",   "int64_t",  PyLong_FromLong,         pylong_as_strictlong>;
  using ulong_cnv  = scalar_converter<ph_ulong_t,    "ulong",  "uint64_t", PyLong_FromUnsignedLong, pylong_or_int_as_ulong>;
  using float_cnv  = scalar_converter<float,         "float",  "float",    PyFloat_FromDouble,      PyFloat_AsDouble>;
  using double_cnv = scalar_converter<double,        "double", "double",   PyFloat_FromDouble,      PyFloat_AsDouble>;
  // clang-format on

  template <typename T, int NpType, fixed_string Name, fixed_string DType, auto FromPy>
  struct vector_converter {
    using vector_ptr = std::shared_ptr<std::vector<T>>;
    static constexpr std::string_view key = DType.view();

    static dcarg to_py(vector_ptr const& v)
    {
      py_gilraii const gil;
      if (!v) {
        Py_INCREF(Py_None);
        return dcarg{Py_None};
      }
      std::array<npy_intp, 1> dims{static_cast<npy_intp>(v->size())};
      PyObject* np_view = PyArray_SimpleNewFromData(1, dims.data(), NpType, v->data());
      if (!np_view) {
        throw std::runtime_error("failed to allocate numpy view object");
      }
      PyArray_CLEARFLAGS(reinterpret_cast<PyArrayObject*>(np_view), NPY_ARRAY_WRITEABLE);
      auto* pyll = reinterpret_cast<py_lifeline_t*>(
        phlex_lifeline_type.tp_new(&phlex_lifeline_type, nullptr, nullptr));
      if (!pyll) {
        Py_DECREF(np_view);
        throw std::runtime_error("failed to allocate lifeline object");
      }
      pyll->source = v;
      pyll->view = np_view; // steals reference
      return dcarg{pyll};
    }

    static vector_ptr from_py(dcarg a)
    {
      py_gilraii const gil;

      auto vec = std::make_shared<std::vector<T>>();
      PyObject* pyobj = a.get<PyObject*>();

      // TODO: because of unresolved ownership issues, copy the full array contents
      if (PyArray_Check(pyobj)) {
        // insist on a C-contiguous, aligned array; in theory it could be copied
        // element-by element, flattening the array, but this copy is already bad
        // enough; can reconsider when we support a non-copying return
        py_owned const carr{PyArray_FROMANY(pyobj, NpType, 0, 0, NPY_ARRAY_IN_ARRAY)};
        if (!carr) {
          std::string msg;
          msg_from_py_error(msg, false);
          Py_DECREF(pyobj);
          throw std::runtime_error("Array conversion error for type " + std::string{Name.view()} +
                                   ": " + msg);
        }

        auto* arr = reinterpret_cast<PyArrayObject*>(carr.get());
        auto const total = static_cast<size_t>(PyArray_SIZE(arr));
        auto* raw = static_cast<T*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);

      } else if (PyList_Check(pyobj)) {
        Py_ssize_t const total = PyList_Size(pyobj);
        vec->reserve(total);
        for (Py_ssize_t i = 0; i < total; ++i) {
          PyObject* item = PyList_GetItem(pyobj, i);
          auto value = static_cast<T>(FromPy(item));
          std::string msg;
          if (msg_from_py_error(msg, true)) {
            Py_DECREF(pyobj);
            throw std::runtime_error("List conversion error for type " + std::string{Name.view()} +
                                     ": " + msg);
          }
          vec->push_back(value);
        }
      } else {
        std::string msg;
        if (!msg_from_py_error(msg, true)) {
          msg = std::string{"expected list or ndarray, got "} + Py_TYPE(pyobj)->tp_name;
        }
        Py_DECREF(pyobj);
        throw std::runtime_error("List conversion error for type " + std::string{Name.view()} +
                                 ": " + msg);
      }

      Py_DECREF(pyobj);
      return vec;
    }

    struct provider : py_callback<dcarg, 1> {
      using base = py_callback<dcarg, 1>;
      using base::base;

      vector_ptr operator()(data_cell_index const& id)
      {
        py_gilraii const gil;
        PyObject* arg0 = wrap_dci(id);
        dcarg const pyres = base::operator()(dcarg{arg0}); // decrefs arg0
        return vector_converter::from_py(pyres);           // decrefs pyres
      }
    };
  };

  // prefer to retain the columnar layout even if that leads to too long lines
  // clang-format off
  using vint_cnv    = vector_converter<std::int32_t,  NPY_INT32,  "vint",    "[int32_t]",  PyLong_AsLong>;
  using vuint_cnv   = vector_converter<std::uint32_t, NPY_UINT32, "vuint",   "[uint32_t]", pylong_or_int_as_ulong>;
  using vlong_cnv   = vector_converter<std::int64_t,  NPY_INT64,  "vlong",   "[int64_t]",  pylong_as_strictlong>;
  using vulong_cnv  = vector_converter<std::uint64_t, NPY_UINT64, "vulong",  "[uint64_t]", pylong_or_int_as_ulong>;
  using vfloat_cnv  = vector_converter<float,         NPY_FLOAT,  "vfloat",  "[float]",    PyFloat_AsDouble>;
  using vdouble_cnv = vector_converter<double,        NPY_DOUBLE, "vdouble", "[double]",   PyFloat_AsDouble>;
  // clang-format on

  // lookup maps of declared converters; need to add any converters below if new
  // ones are added above
  template <typename... Cs>
  struct converter_lookup {
    template <typename F>
    static bool dispatch(std::string_view key, F f)
    {
      return ((key == Cs::key ? (f.template operator()<Cs>(), true) : false) || ...);
    }
  };

  using scalar_converters =
    converter_lookup<bool_cnv, int_cnv, uint_cnv, long_cnv, ulong_cnv, float_cnv, double_cnv>;
  using vector_converters =
    converter_lookup<vint_cnv, vuint_cnv, vlong_cnv, vulong_cnv, vfloat_cnv, vdouble_cnv>;

  // returns the dtype suffix (e.g. "[float]") from a collection type string (e.g. "list[float]"),
  // or std::nullopt if the string contains no '[' character
  inline std::optional<std::string_view> collection_dtype(std::string const& type_name)
  {
    auto const pos = type_name.rfind('[');
    if (pos == std::string::npos) {
      return std::nullopt;
    }
    return std::string_view{type_name}.substr(pos);
  }

  // C++ type to converter dispatch helper; this adds error Python handling to the basic
  // lookup for unsupported types
  template <typename ScalarF, typename VectorF>
  bool dispatch_type(std::string const& type,
                     char const* what,
                     ScalarF on_scalar,
                     VectorF on_vector)
  {
    if (is_collection_type(type)) {
      auto const dtype = collection_dtype(type);
      if (dtype && vector_converters::dispatch(*dtype, on_vector)) {
        return true;
      }
      PyErr_Format(PyExc_TypeError, "unsupported collection %s type \"%s\"", what, type.c_str());
      return false;
    }

    if (scalar_converters::dispatch(type, on_scalar)) {
      return true;
    }

    PyErr_Format(PyExc_TypeError, "unsupported %s type \"%s\"", what, type.c_str());
    return false;
  }

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

namespace {

  // NOLINTNEXTLINE(readability-function-size)
  PyObject* parse_args(PyObject* args,
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
    nconcur = nconcurrency > 0 ? concurrency(nconcurrency) : concurrency::serial;

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
    } else if (output_suffixes.empty() && PyErr_Occurred()) {
      return nullptr; // Python error already set
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
      // allow fewer selectors than types if there are sufficient optional
      // parameters on the Python side
      bool optok = false;
      if (input_selectors.size() < input_types.size()) {
        static PyObject* opt_counter = nullptr;
        if (!opt_counter) {
          opt_counter = import_attr("phlex", "count_optional_arguments");

          // LCOV_EXCL_START
          // this would only fail if the phlex installation were broken and
          // only exists to get a proper error message instead of a segfault
          // in that rather unlikely case
          if (!opt_counter) {
            PyErr_Clear();
          }
          // LCOV_EXCL_STOP
        }

        if (opt_counter) {
          PyObject* optcnt = PyObject_CallOneArg(opt_counter, callable);
          if (optcnt) {
            long const l = PyLong_AsLong(optcnt);
            Py_DECREF(optcnt);
            // I'd use -1l if clang-tidy would allow it, but it insists on -1L ...
            if (l != static_cast<long>(-1)) {
              if ((l + input_selectors.size()) >= input_types.size()) {
                optok = true;
              }
            } else {
              PyErr_Clear();
            }
          }
          // LCOV_EXCL_START
          else {
            PyErr_Clear(); // count_optional_arguments doesn't raise
          }
          // LCOV_EXCL_STOP
        }
      }

      if (!optok) {
        PyErr_Format(
          PyExc_TypeError,
          "number of inputs (%zu; %s) does not match number of annotation types (%zu; %s)",
          input_selectors.size(),
          stringify(input_selectors).c_str(),
          input_types.size(),
          stringify(input_types).c_str());
        return nullptr;
      }
    }

    // special case of Phlex Variant wrapper
    callable = unwrap_variant(callable);

    // no common errors detected; actual registration may have more checks
    return callable;
  }

  // NOLINTNEXTLINE(readability-function-size)
  bool insert_input_converters(py_phlex_module* mod,
                               std::string const& cname, // TODO: shared_ptr<PyObject>
                               std::vector<product_selector> const& input_selectors,
                               std::vector<std::string> const& input_types,
                               bool ispy,
                               concurrency nc)
  {
    // insert input converter nodes, following the requested C++ type, into the graph
    for (auto const [i, inp_pq, inp_type] :
         std::views::zip(std::views::iota(size_t{}), input_selectors, input_types)) {
      std::string const pyname = input_converter_name(cname, i);
      std::string const output = converted_suffix(inp_pq);
      bool const ok = dispatch_type(
        inp_type,
        "input",
        [&]<typename C>() {
          insert_converter(mod, pyname, ispy ? &C::to_py : &C::to_dcarg, inp_pq, output, nc);
        },
        [&]<typename C>() { insert_converter(mod, pyname, &C::to_py, inp_pq, output, nc); });

      if (!ok) {
        return false;
      }
    }

    return true;
  }

  bool insert_output_converter(py_phlex_module* mod,
                               std::string const& cname,
                               product_selector const& out_pq,
                               std::string const& out_type,
                               std::string const& output,
                               bool ispy,
                               concurrency nc)
  {
    // insert output converter node, following the requested C++ type, into the graph
    return dispatch_type(
      out_type,
      "output",
      [&]<typename C>() {
        insert_converter(mod, cname, ispy ? &C::from_py : &C::from_dcarg, out_pq, output, nc);
      },
      [&]<typename C>() { insert_converter(mod, cname, &C::from_py, out_pq, output, nc); });
  }

  void* numba_function_address(py_owned const& callable)
  {
    // Detect Numba and extract its C function pointer; otherwise use the Python dispatcher.
    if (!is_numba_cfunc(callable.get())) {
      return nullptr;
    }

    PyObject* pyaddr = PyObject_GetAttrString(callable.get(), "address");
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

  std::optional<identifier> transform_output_layer(
    std::string const& name, std::vector<product_selector> const& input_selectors)
  {
    // if a layer was provided, we'll re-use it for the intermediate Python products,
    // otherwise also specify no layer for the intermediates (TODO: it may be worthwhile
    // to explore using a "workspace" layer)
    std::optional<identifier> output_layer;
    // note: the following treats the first layer as special (as in, if the first layer
    // is optional, but the next one is not, the check will succeed), but that's fine
    // as for now such mixing isn't supported by the product selector
    if (input_selectors[0].layer) {
      output_layer = static_cast<identifier>(input_selectors[0].layer);
      // TODO: it's not clear what the output layer will be if the input layers are not
      // all the same, so for now, simply raise an error if their is any ambiguity
      for (auto const& iq_pq : input_selectors | std::views::drop(1)) {
        if (static_cast<identifier>(iq_pq.layer) != output_layer.value()) {
          PyErr_Format(PyExc_ValueError, "transform %s output layer is ambiguous", name.c_str());
          return std::nullopt; // error return
        }
      }
    }
    return output_layer; // not an error return if nullopt
  }

  bool validate_transform_output(std::string const& name,
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

  bool validate_numba_transform_types(std::string const& name,
                                      std::vector<std::string> const& input_types,
                                      std::string const& output_type)
  {
    // TODO: remove this temporary restriction once Numba vector support is available.
    // LCOV_EXCL_START
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

  product_selector converted_input_selector(std::string const& name,
                                            std::size_t index,
                                            product_selector const& selector)
  {
    std::string const suffix = converted_suffix(selector);
    return {.creator = identifier(input_converter_name(name, index)),
            .layer = selector.layer,
            .suffix = identifier(suffix)};
  }

  template <size_t N, typename Cf>
  bool unroll_switch(size_t rt_size, Cf&& func)
  {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      // 1-based sequence (all computational nodes have an input, or they can't be scheduled),
      // with the fold expression short-circuited using ||

      // clang-tidy is incorrect here, b/c the condition "rt_size == (Is + 1)" is only ever
      // true once, so the forward is only called once, and func is never used after move
      // NOLINTBEGIN(bugprone-use-after-move)
      bool const matched =
        (... ||
         ((rt_size == (Is + 1)) ? (std::forward<Cf>(func)(std::make_index_sequence<Is + 1>{}), true)
                                : false));
      // NOLINTEND(bugprone-use-after-move)

      return matched;
    }(std::make_index_sequence<N>{});
  }

  std::optional<product_selector> register_transform_callback(
    py_phlex_module* mod,
    PyObject* callable,
    void* ccallf,
    std::string const& name,
    std::vector<product_selector> const& input_selectors,
    std::string const& output_type,
    std::string const& output_suffix,
    std::optional<identifier> output_layer,
    concurrency nconcur)
  {
    // Only a single output is supported until typed tuple conversion is implemented.
    std::string const pyname = "py_" + name;
    std::string const pyoutput = output_suffix + "_py";
    auto output_selector = product_selector{.creator = identifier(pyname),
                                            .layer = std::move(output_layer),
                                            .suffix = identifier(pyoutput)};

    auto register_n_args = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      constexpr std::size_t n = sizeof...(Is);
      auto add = [&](auto const& callback) {
        mod->ph_module->transform(pyname, callback, nconcur)
          .input_family(converted_input_selector(name, Is, input_selectors[Is])...)
          .output_product_suffixes(pyoutput);
      };
      if (ccallf) {
        add(jit_callback<dcarg, n>{callable, ccallf, output_type});
      } else {
        add(py_callback<dcarg, n>{callable});
      }
    };

    if (unroll_switch<max_supported_args>(input_selectors.size(), register_n_args)) {
      return output_selector;
    }

    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    return std::nullopt;
  }

  PyObject* md_transform(py_phlex_module* mod, PyObject* args, PyObject* kwds)
  {
    // Register a python algorithm by adding the necessary intermediate converter
    // nodes going from C++ to PyObject* and back.

    std::string cname;
    std::vector<product_selector> input_selectors;
    std::vector<std::string> input_types;
    std::vector<std::string> output_suffixes;
    std::vector<std::string> output_types;
    concurrency nconcur(-1);
    py_owned const callable{parse_args(
      args, kwds, cname, input_selectors, input_types, output_suffixes, output_types, nconcur)};

    // verify properties and invariants; helpers will set errors as appropriate

    if (!callable) {
      return nullptr;
    }

    void* ccallf = numba_function_address(callable);
    if (!validate_transform_output(cname, output_types, output_suffixes)) {
      return nullptr;
    }

    auto const output_layer = transform_output_layer(cname, input_selectors);
    // transform_output_layer only exists to make clang-tidy happy and if it
    // doesn't return an output_layer, then that's not necessarily an error as
    // layer are optional: an actual error is communicated around the back using
    // a Python exception that is reported to the caller in the usual way
    if (!output_layer && PyErr_Occurred()) {
      return nullptr;
    }

    if (!insert_input_converters(mod, cname, input_selectors, input_types, !ccallf, nconcur)) {
      return nullptr;
    }

    std::string const& out_type = output_types[0];
    if (ccallf && !validate_numba_transform_types(cname, input_types, out_type)) {
      return nullptr;
    }

    auto const output_selector = register_transform_callback(mod,
                                                             callable.get(),
                                                             ccallf,
                                                             cname,
                                                             input_selectors,
                                                             out_type,
                                                             output_suffixes[0],
                                                             output_layer,
                                                             nconcur);
    if (!output_selector) {
      return nullptr;
    }

    // insert output converter node into the graph
    std::string const& output = output_suffixes[0];
    if (!insert_output_converter(
          mod, cname, *output_selector, out_type, output, !ccallf, nconcur)) {
      return nullptr;
    }

    Py_RETURN_NONE;
  }

  // NOLINTNEXTLINE(readability-function-size)
  PyObject* md_observe(py_phlex_module* mod, PyObject* args, PyObject* kwds)
  {
    // Register a python observer by adding the necessary intermediate converter
    // nodes going from C++ to PyObject* and back.

    std::string cname;
    std::vector<product_selector> input_selectors;
    std::vector<std::string> input_types;
    std::vector<std::string> output_suffixes;
    std::vector<std::string> output_types;
    concurrency nconcur(-1);
    py_owned const callable{parse_args(
      args, kwds, cname, input_selectors, input_types, output_suffixes, output_types, nconcur)};

    // verify properties and invariants; helpers will set errors as appropriate

    if (!callable) {
      return nullptr;
    }

    // detect numba and extract C function pointer if any, else use default Python
    // callable dispatcher
    void* ccallf = numba_function_address(callable);

    if (!output_types.empty()) {
      PyErr_Format(PyExc_TypeError,
                   "an observer should not have an output type (got: \"%s\")",
                   output_types[0].c_str());
      return nullptr;
    }

    if (!insert_input_converters(mod, cname, input_selectors, input_types, !ccallf, nconcur)) {
      return nullptr;
    }

    // register Python observer callbacks
    auto observe_n_args = [&]<size_t... Is>(std::index_sequence<Is...>) {
      constexpr size_t n = sizeof...(Is);

      auto insert_observe_for_callback = [&](auto& cb) {
        mod->ph_module->observe(cname, cb, nconcur)
          .input_family(converted_input_selector(cname, Is, input_selectors[Is])...);
      };

      if (ccallf) {
        jit_callback<void, n> cb{callable.get(), ccallf, "void"};
        insert_observe_for_callback(cb);
      } else {
        py_callback<void, n> cb{callable.get()};
        insert_observe_for_callback(cb);
      }
    };

    if (!unroll_switch<max_supported_args>(input_selectors.size(), observe_n_args)) {
      PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
      return nullptr;
    }

    Py_RETURN_NONE;
  }

  // PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  std::array<PyMethodDef, 3> md_methods{
    {{.ml_name = "transform",
      .ml_meth = reinterpret_cast<PyCFunction>(md_transform),
      .ml_flags = METH_VARARGS | METH_KEYWORDS,
      .ml_doc = "register a Python transform"},
     {.ml_name = "observe",
      .ml_meth = reinterpret_cast<PyCFunction>(md_observe),
      .ml_flags = METH_VARARGS | METH_KEYWORDS,
      .ml_doc = "register a Python observer"},
     {.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr}}};

} // namespace

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

namespace {

  struct provider_registration {
    PyObject* callable;
    PyObject* output;
    std::string name;
    std::string output_type;
  };

  // The source wrapper remains here to share the Python converter implementations.
  // TODO: move it to a dedicated source-wrapper translation unit.
  std::optional<provider_registration> parse_provider_registration(PyObject* args, PyObject* kwds)
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

  bool register_provider(py_phlex_source* src,
                         std::string const& name,
                         PyObject* callable,
                         algorithm_name const& creator,
                         identifier const& suffix,
                         identifier const& layer,
                         std::string const& output_type)
  {
    auto provide = [&]<typename C>() {
      src->ph_source->provide(name, typename C::provider{callable})
        .output_product(creator, suffix, layer);
    };
    return dispatch_type(output_type, "output", provide, provide);
  }

  PyObject* sc_provide(py_phlex_source* src, PyObject* args, PyObject* kwds)
  {
    auto registration = parse_provider_registration(args, kwds);
    if (!registration) {
      return nullptr;
    }

    // special case of Phlex Variant wrapper
    py_owned const callable{unwrap_variant(registration->callable)};

    // translate and validate the output "selectors"
    // Since a selector in Python is just a dictionary, it isn't called out in the user
    // API as a selector
    auto opq = validate_selector(registration->output, false);
    if (!opq.has_value()) {
      // validate_selector has set a python exception with details about the error
      return nullptr;
    }

    algorithm_name const creator = algorithm_name::create(std::string_view(*opq.value().creator));
    identifier const layer = opq.value().layer;
    identifier const suffix = opq.value().suffix.value_or("");

    // Provider callbacks retain the callable for the graph lifetime. A translator node
    // cannot be used here because sources do not have a module for adding one.
    // TODO: callable cleanup is deferred until a Phlex shutdown procedure exists.
    bool const registered = register_provider(
      src, registration->name, callable.get(), creator, suffix, layer, registration->output_type);
    if (!registered) {
      if (!PyErr_Occurred()) {
        PyErr_Format(
          PyExc_TypeError, "unsupported output type \"%s\"", registration->output_type.c_str());
      }
      return nullptr;
    }

    Py_RETURN_NONE;
  }

  // PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  std::array<PyMethodDef, 2> sc_methods{
    {{.ml_name = "provide",
      .ml_meth = reinterpret_cast<PyCFunction>(sc_provide),
      .ml_flags = METH_VARARGS | METH_KEYWORDS,
      .ml_doc = "register a Python provider"},
     {.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr}}};

} // namespace

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

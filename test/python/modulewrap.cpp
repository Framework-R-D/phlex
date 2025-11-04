#include "phlex/module.hpp"
#include "wrap.hpp"

#include <functional>

using namespace phlex::experimental;

// Simple phlex module wrapper
struct phlex::experimental::py_phlex_module {
  PyObject_HEAD phlex_module_t* ph_module;
};

PyObject* phlex::experimental::wrap_module(phlex_module_t* module_)
{
  if (!module_) {
    PyErr_SetString(PyExc_ValueError, "provided module is null");
    return nullptr;
  }

  py_phlex_module* pymod = PyObject_New(py_phlex_module, &PhlexModule_Type);
  pymod->ph_module = module_;

  return (PyObject*)pymod;
}

namespace {

  // callable object managing the callback
  template <size_t N>
  struct py_callback {
    PyObject const* m_callable; // borrowed

    template <typename... Args>
    intptr_t call(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result =
        PyObject_CallFunctionObjArgs((PyObject*)m_callable, (PyObject*)args..., nullptr);

      decref_all(args...);

      if (!result)
        throw_runtime_error_from_py_error(false);

      return (intptr_t)result;
    }

    template <typename... Args>
    void callv(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result =
        PyObject_CallFunctionObjArgs((PyObject*)m_callable, (PyObject*)args..., nullptr);
      Py_XDECREF(result);

      decref_all(args...);

      if (!result)
        throw_runtime_error_from_py_error(false);
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

  static std::vector<std::string> cseq(PyObject* coll)
  {
    size_t len = (size_t)PySequence_Size(coll);
    std::vector<std::string> cargs{len};

    for (size_t i = 0; i < len; ++i) {
      PyObject* item = PySequence_GetItem(coll, i);
      if (item) {
        char const* p = PyUnicode_AsUTF8(item);
        if (p) {
          Py_ssize_t sz = PyUnicode_GetLength(item);
          cargs[i].assign(p, (std::string::size_type)sz);
        }
        Py_DECREF(item);

        if (!p) {
          PyErr_Format(PyExc_TypeError, "could not convert item %d to string", (int)i);
          break;
        }
      } else
        break; // Python error already set
    }

    return cargs;
  }

} // unnamed namespace

namespace {

  static std::string annotation_as_text(PyObject* pyobj)
  {
    char const* cstr = nullptr;
    if (!PyUnicode_Check(pyobj)) {
      PyObject* pystr = PyObject_GetAttrString(pyobj, "__name__"); // eg. for classes
      if (!pystr) {
        PyErr_Clear();
        pystr = PyObject_Str(pyobj);
      }

      cstr = PyUnicode_AsUTF8(pystr);
      Py_DECREF(pystr);
      return cstr;
    }
    cstr = PyUnicode_AsUTF8(pyobj);

    if (!cstr)
      return "";
    return cstr;
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
    if (!PyLong_Check(pyobject)) {
      PyErr_SetString(PyExc_TypeError, "int/long conversion expects an integer object");
      return (long)-1;
    }
    return (long)PyLong_AsLong(pyobject); // already does long range check
  }

  static unsigned long pylong_or_int_as_ulong(PyObject* pyobject)
  {
    // convert <pybject> to C++ unsigned long, with bounds checking, allow int -> ulong.
    if (PyFloat_Check(pyobject)) {
      PyErr_SetString(PyExc_TypeError, "can\'t convert float to unsigned long");
      return (unsigned long)-1;
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
    Py_DECREF(pyobj);                                                                              \
    return i;                                                                                      \
  }

  BASIC_CONVERTER(bool, bool, PyBool_FromLong, pylong_as_bool)
  BASIC_CONVERTER(int, int, PyLong_FromLong, PyLong_AsLong)
  BASIC_CONVERTER(uint, unsigned int, PyLong_FromLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(long, long, PyLong_FromLong, pylong_as_strictlong)
  BASIC_CONVERTER(ulong, unsigned long, PyLong_FromUnsignedLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(float, float, PyFloat_FromDouble, PyFloat_AsDouble)
  BASIC_CONVERTER(double, double, PyFloat_FromDouble, PyFloat_AsDouble)

} // unnamed namespace

#define INSERT_INPUT_CONVERTER(name, inp)                                                          \
  mod->ph_module->transform("py" #name "_" + inp, name##_to_py, concurrency::serial)               \
    .input_family(inp)                                                                             \
    .output_products(inp + "py")

#define INSERT_OUTPUT_CONVERTER(name, outp)                                                        \
  mod->ph_module->transform(#name "py_" + outp, py_to_##name, concurrency::serial)                 \
    .input_family("py" + output)                                                                   \
    .output_products(output)

static PyObject* md_register(py_phlex_module* mod, PyObject* args, PyObject* /*kwds*/)
{
  // Register a python algorithm by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  if (PyTuple_Size(args) < 3) {
    PyErr_SetString(PyExc_TypeError, "expected arguments: callable, inputs, outputs");
    return nullptr;
  }

  PyObject* callable = PyTuple_GetItem(args, 0);
  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "provided algorithm is not callable");
    return nullptr;
  }

  // retrieve function name and argument types
  PyObject* pyname = PyObject_GetAttrString(callable, "__name__");
  if (!pyname) {
    // AttributeError already set
    return nullptr;
  }
  std::string cname = PyUnicode_AsUTF8(pyname);
  Py_DECREF(pyname);

  PyObject* inputs = PyTuple_GetItem(args, 1);
  PyObject* outputs = PyTuple_GetItem(args, 2);
  if (!(PySequence_Check(inputs) && PySequence_Check(outputs))) {
    PyErr_SetString(PyExc_TypeError, "inputs and outputs need to be sequences");
    return nullptr;
  }

  // convert input and output declarations, to be able to pass them to Phlex
  auto const& cinputs = cseq(inputs);
  auto const& coutputs = cseq(outputs);
  if (coutputs.size() > 1) {
    PyErr_SetString(PyExc_TypeError, "only a single output supported");
    return nullptr;
  }
  std::string const& output = coutputs[0];

  std::vector<std::string> input_types;
  input_types.reserve(cinputs.size());
  std::string output_type; // TODO: accept a tuple (see also above)

  PyObject* annot = PyObject_GetAttrString(callable, "__annotations__");
  if (annot && PyDict_Check(annot) && PyDict_Size(annot)) {
    PyObject* ret = PyDict_GetItemString(annot, "return");
    if (ret)
      output_type = annotation_as_text(ret);

    // dictionary is ordered with return last if provide (note: the keys here
    // could be used as input labels, instead of the ones from the configuration,
    // but that is probably not practical in actual use, so they are ignored)
    PyObject* values = PyDict_Values(annot);
    for (Py_ssize_t i = 0; i < (PyList_GET_SIZE(values) - (ret ? 1 : 0)); ++i) {
      PyObject* item = PyList_GET_ITEM(values, i);
      input_types.emplace_back(annotation_as_text(item));
    }
    Py_DECREF(values);
  }
  Py_XDECREF(annot);

  if (input_types.size() != cinputs.size()) {
    PyErr_SetString(PyExc_TypeError, "annotions not found or unknown formatting");
    return nullptr;
  }

  // insert input converter nodes into the graph
  for (size_t i = 0; i < (size_t)cinputs.size(); ++i) {
    // TODO: this seems overly verbose and inefficient, but the function needs
    // to be properly types, so every option is made explicit
    auto const& inp = cinputs[i];
    auto const& inp_type = input_types[i];

    if (inp_type == "bool")
      INSERT_INPUT_CONVERTER(bool, inp);
    else if (inp_type == "int")
      INSERT_INPUT_CONVERTER(int, inp);
    else if (inp_type == "unsigned int")
      INSERT_INPUT_CONVERTER(uint, inp);
    else if (inp_type == "long")
      INSERT_INPUT_CONVERTER(long, inp);
    else if (inp_type == "unsigned long")
      INSERT_INPUT_CONVERTER(ulong, inp);
    else if (inp_type == "float")
      INSERT_INPUT_CONVERTER(float, inp);
    else if (inp_type == "double")
      INSERT_INPUT_CONVERTER(double, inp);
  }

  // register Python algorithm
  if (cinputs.size() == 1) {
    if (!output_type.empty()) {
      auto* pyc = new py_callback_1{callable}; // TODO: leaks, but has program lifetime
      mod->ph_module->transform(cname, *pyc, concurrency::serial)
        .input_family(cinputs[0] + "py")
        .output_products("py" + output);
    } else {
      auto* pyc = new py_callback_1v{callable}; // id.
      mod->ph_module->observe(cname, *pyc, concurrency::serial).input_family(cinputs[0] + "py");
    }
  } else if (cinputs.size() == 2) {
    if (!output_type.empty()) {
      auto* pyc = new py_callback_2{callable};
      mod->ph_module->transform(cname, *pyc, concurrency::serial)
        .input_family(cinputs[0] + "py", cinputs[1] + "py")
        .output_products("py" + output);
    } else {
      auto* pyc = new py_callback_2v{callable};
      mod->ph_module->observe(cname, *pyc, concurrency::serial)
        .input_family(cinputs[0] + "py", cinputs[1] + "py");
    }
  } else if (cinputs.size() == 3) {
    if (!output_type.empty()) {
      auto* pyc = new py_callback_3{callable};
      mod->ph_module->transform(cname, *pyc, concurrency::serial)
        .input_family(cinputs[0] + "py", cinputs[1] + "py", cinputs[2] + "py")
        .output_products("py" + output);
    } else {
      auto* pyc = new py_callback_3v{callable};
      mod->ph_module->observe(cname, *pyc, concurrency::serial)
        .input_family(cinputs[0] + "py", cinputs[1] + "py", cinputs[2] + "py");
    }
  } else {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    return nullptr;
  }

  if (!output_type.empty()) {
    // insert output converter node into the graph (TODO: same as above; these
    // are explicit b/c of the templates only)
    if (output_type == "bool")
      INSERT_OUTPUT_CONVERTER(bool, output);
    else if (output_type == "int")
      INSERT_OUTPUT_CONVERTER(int, output);
    else if (output_type == "unsigned int")
      INSERT_OUTPUT_CONVERTER(uint, output);
    else if (output_type == "long")
      INSERT_OUTPUT_CONVERTER(long, output);
    else if (output_type == "unsigned long")
      INSERT_OUTPUT_CONVERTER(ulong, output);
    else if (output_type == "float")
      INSERT_OUTPUT_CONVERTER(float, output);
    else if (output_type == "double")
      INSERT_OUTPUT_CONVERTER(double, output);
  }

  Py_RETURN_NONE;
}

static PyMethodDef md_methods[] = {
  {(char*)"register", (PyCFunction)md_register, METH_VARARGS, (char*)"register a Python algorithm"},
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

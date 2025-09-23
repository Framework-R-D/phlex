#include "phlex/module.hpp"
#include "wrap.hpp"

#include <functional>


using namespace phlex::experimental;

namespace {
class PyGILRAII {
  PyGILState_STATE m_GILState;
public:
  PyGILRAII() : m_GILState(PyGILState_Ensure()) {}
  ~PyGILRAII() { PyGILState_Release(m_GILState); }
};
}

// Simple phlex module wrapper
struct phlex::experimental::py_phlex_module {
  PyObject_HEAD
  phlex_module_t* ph_module;
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
// TODO: any more compact way? variadics don't seem to work b/c the function signature
// needs to be explicit for the registration to derive the argument types
struct py_callback {
  PyObject const* m_callable;     // borrowed
};

struct py_callback_1 : public py_callback {
  intptr_t operator()(intptr_t arg0) {
    PyGILRAII gil;
    PyObject* result = PyObject_CallFunctionObjArgs(
      (PyObject*)m_callable, (PyObject*)arg0, nullptr);
    Py_DECREF(arg0);
    return (intptr_t)result;
  }
};

struct py_callback_2 : public py_callback {
  intptr_t operator()(intptr_t arg0, intptr_t arg1) {
    PyGILRAII gil;
    PyObject* result = PyObject_CallFunctionObjArgs(
      (PyObject*)m_callable, (PyObject*)arg0, (PyObject*)arg1, nullptr);
    Py_DECREF(arg1); Py_DECREF(arg0);
    return (intptr_t)result;
  }
};

struct py_callback_3 : public py_callback {
  intptr_t operator()(intptr_t arg0, intptr_t arg1, intptr_t arg2) {
    PyGILRAII gil;
    PyObject* result = PyObject_CallFunctionObjArgs(
      (PyObject*)m_callable, (PyObject*)arg0, (PyObject*)arg1, (PyObject*)arg2, nullptr);
    Py_DECREF(arg2); Py_DECREF(arg1); Py_DECREF(arg0);
    return (intptr_t)result;
  }
};

// converters
static intptr_t pyint(int i) { PyGILRAII gil; return (intptr_t)PyLong_FromLong(i); }
static int intpy(intptr_t pyobj) {
  PyGILRAII gil;
  int i = (int)PyLong_AsLong((PyObject*)pyobj);
  Py_DECREF(pyobj);
  return i;
}

static std::vector<std::string> cseq(PyObject* coll) {
  size_t len = (size_t)PySequence_Size(coll);
  std::vector<std::string> cargs{len};

  for (size_t i = 0; i < len; ++i) {
    PyObject* item = PySequence_GetItem(coll, i);
    if (item) {
      const char* p = PyUnicode_AsUTF8(item);
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
      break;       // Python error already set
  }

  return cargs;
}

} // unnamed namespace


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

  PyObject* inputs = PyTuple_GetItem(args, 1);
  PyObject* outputs = PyTuple_GetItem(args, 2);
  if (!(PySequence_Check(inputs) && PySequence_Check(outputs))) {
    PyErr_SetString(PyExc_TypeError, "inputs and outputs need to be sequences");
    return nullptr;
  }

  // convert input and output declarations, to be able to pass them to Phlex
  auto const& cinputs  = cseq(inputs);
  auto const& coutputs = cseq(outputs);
  if (coutputs.size() > 1) {
    PyErr_SetString(PyExc_TypeError, "only a single output supported");
    return nullptr;
  }
  const std::string& output = coutputs[0];

  // insert input converter nodes into the graph
  for (auto const& inp: cinputs) {
    mod->ph_module->with("pyint_"+inp, pyint, concurrency::serial).
      transform(inp).
      to("py"+inp);
  }

  // register Pythobn algorithm
  if (cinputs.size() == 1) {
    auto* pyc = new py_callback_1{callable};     // TODO: leaks, but has program lifetime
    mod->ph_module->with("add", *pyc, concurrency::serial).
      transform("py"+cinputs[0]).
      to("py"+output);
  } else if (cinputs.size() == 2) {
    auto* pyc = new py_callback_2{callable};     // TODO: id.
    mod->ph_module->with("add", *pyc, concurrency::serial).
      transform("py"+cinputs[0], "py"+cinputs[1]).
      to("py"+output);
  } else if (cinputs.size() == 3) {
    auto* pyc = new py_callback_3{callable};     // TODO: id
    mod->ph_module->with("add", *pyc, concurrency::serial).
      transform("py"+cinputs[0], "py"+cinputs[1], "py"+cinputs[2]).
      to("py"+output);
  } else {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    return nullptr;
  }

  // insert output converter node into the graph
  mod->ph_module->with("intpy_"+output, intpy, concurrency::serial).
    transform("py"+output).
    to(output);

  Py_RETURN_NONE;
}


static PyMethodDef md_methods[] = {
    {(char*)"register", (PyCFunction)md_register, METH_VARARGS,
      (char*)"register a Python algorithm"},
    {(char*)nullptr, nullptr, 0, nullptr}
};


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
    , 0                           // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
    , 0                           // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
    , 0                           // tp_versions_used
#endif
};

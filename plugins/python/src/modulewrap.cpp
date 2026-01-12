#include "phlex/module.hpp"
#include "wrap.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL phlex_ARRAY_API
#include <numpy/arrayobject.h>

using namespace phlex::experimental;
using phlex::concurrency;
using phlex::product_query;

struct PyObjectDeleter {
  void operator()(PyObject* p) const
  {
    if (p) {
      phlex::experimental::PyGILRAII gil;
      Py_DECREF(p);
    }
  }
};
using PyObjectPtr = std::shared_ptr<PyObject>;

// TODO: the layer is currently hard-wired and should come from the product
// specification instead, but that doesn't exist in Python yet.
static std::string const LAYER = "job";

// Simple phlex module wrapper
// clang-format off
struct phlex::experimental::py_phlex_module {
  PyObject_HEAD
  phlex_module_t* ph_module;
};
// clang-format on

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

  // TODO: wishing for std::views::join_with() in C++23, but until then:
  static std::string stringify(std::vector<std::string>& v)
  {
    std::ostringstream oss;
    if (!v.empty()) {
      oss << v.front();
      for (std::size_t i = 1; i < v.size(); ++i) {
        oss << ", " << v[i];
      }
    }
    return oss.str();
  }

  static inline PyObject* lifeline_transform(PyObject* arg)
  {
    if (Py_TYPE(arg) == &PhlexLifeline_Type) {
      return ((py_lifeline_t*)arg)->m_view;
    }
    return arg;
  }

  // callable object managing the callback
  template <size_t N>
  struct py_callback {
    PyObject const* m_callable; // owned

    py_callback(PyObject const* callable)
    {
      Py_INCREF(callable);
      m_callable = callable;
    }
    py_callback(py_callback const& pc)
    {
      Py_INCREF(pc.m_callable);
      m_callable = pc.m_callable;
    }
    py_callback& operator=(py_callback const& pc)
    {
      if (this != &pc) {
        Py_INCREF(pc.m_callable);
        m_callable = pc.m_callable;
      }
      return *this;
    }
    ~py_callback() { Py_DECREF(m_callable); }

    template <typename... Args>
    PyObjectPtr call(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result = PyObject_CallFunctionObjArgs(
        (PyObject*)m_callable, lifeline_transform(args.get())..., nullptr);

      std::string error_msg;
      if (!result) {
        if (!msg_from_py_error(error_msg))
          error_msg = "Unknown python error";
      }

      if (!error_msg.empty())
        throw std::runtime_error(error_msg.c_str());

      return PyObjectPtr(result, PyObjectDeleter());
    }

    template <typename... Args>
    void callv(Args... args)
    {
      static_assert(sizeof...(Args) == N, "Argument count mismatch");

      PyGILRAII gil;

      PyObject* result =
        PyObject_CallFunctionObjArgs((PyObject*)m_callable, (PyObject*)args.get()..., nullptr);

      std::string error_msg;
      if (!result) {
        if (!msg_from_py_error(error_msg))
          error_msg = "Unknown python error";
      } else
        Py_DECREF(result);

      if (!error_msg.empty())
        throw std::runtime_error(error_msg.c_str());
    }
  };

  // use explicit instatiations to ensure that the function signature can
  // be derived by the graph builder
  struct py_callback_1 : public py_callback<1> {
    PyObjectPtr operator()(PyObjectPtr arg0) { return call(arg0); }
  };

  struct py_callback_2 : public py_callback<2> {
    PyObjectPtr operator()(PyObjectPtr arg0, PyObjectPtr arg1) { return call(arg0, arg1); }
  };

  struct py_callback_3 : public py_callback<3> {
    PyObjectPtr operator()(PyObjectPtr arg0, PyObjectPtr arg1, PyObjectPtr arg2)
    {
      return call(arg0, arg1, arg2);
    }
  };

  struct py_callback_1v : public py_callback<1> {
    void operator()(PyObjectPtr arg0) { callv(arg0); }
  };

  struct py_callback_2v : public py_callback<2> {
    void operator()(PyObjectPtr arg0, PyObjectPtr arg1) { callv(arg0, arg1); }
  };

  struct py_callback_3v : public py_callback<3> {
    void operator()(PyObjectPtr arg0, PyObjectPtr arg1, PyObjectPtr arg2)
    {
      callv(arg0, arg1, arg2);
    }
  };

  static std::vector<std::string> cseq(PyObject* coll)
  {
    size_t len = coll ? (size_t)PySequence_Size(coll) : 0;
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
    std::string ann;
    if (!PyUnicode_Check(pyobj)) {
      PyObject* pystr = PyObject_GetAttrString(pyobj, "__name__"); // eg. for classes
      if (!pystr) {
        PyErr_Clear();
        pystr = PyObject_Str(pyobj);
      }

      char const* cstr = PyUnicode_AsUTF8(pystr);
      if (cstr)
        ann = cstr;
      Py_DECREF(pystr);

      // for numpy typing, there's no useful way of figuring out the type from the
      // name of the type, only from its string representation, so fall through and
      // let this method return str()
      if (ann != "ndarray" && ann != "list")
        return ann;

      // start over for numpy type using result from str()
      pystr = PyObject_Str(pyobj);
      cstr = PyUnicode_AsUTF8(pystr);
      if (cstr) // if failed, ann will remain "ndarray"
        ann = cstr;
      Py_DECREF(pystr);
      return ann;
    }

    // unicode object, i.e. string name of the type
    char const* cstr = PyUnicode_AsUTF8(pyobj);
    if (cstr)
      ann = cstr;
    else
      PyErr_Clear();

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
  static PyObjectPtr name##_to_py(cpptype a)                                                       \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
    return PyObjectPtr(topy(a), PyObjectDeleter());                                                \
  }                                                                                                \
                                                                                                   \
  static cpptype py_to_##name(PyObjectPtr pyobj)                                                   \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
    cpptype i = (cpptype)frompy(pyobj.get());                                                      \
    return i;                                                                                      \
  }

  BASIC_CONVERTER(bool, bool, PyBool_FromLong, pylong_as_bool)
  BASIC_CONVERTER(int, int, PyLong_FromLong, PyLong_AsLong)
  BASIC_CONVERTER(uint, unsigned int, PyLong_FromLong, pylong_or_int_as_ulong)
  BASIC_CONVERTER(long, long, PyLong_FromLong, pylong_as_strictlong)
  // BASIC_CONVERTER(ulong, unsigned long, PyLong_FromUnsignedLong, pylong_or_int_as_ulong)
  static PyObjectPtr ulong_to_py(unsigned long a)
  {
    PyGILRAII gil;
    return PyObjectPtr(PyLong_FromUnsignedLong(a), PyObjectDeleter());
  }
  static unsigned long py_to_ulong(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    unsigned long i = (unsigned long)pylong_or_int_as_ulong(pyobj.get());
    return i;
  }
  BASIC_CONVERTER(float, float, PyFloat_FromDouble, PyFloat_AsDouble)
  BASIC_CONVERTER(double, double, PyFloat_FromDouble, PyFloat_AsDouble)

#define VECTOR_CONVERTER(name, cpptype, nptype)                                                    \
  static PyObjectPtr name##_to_py(std::shared_ptr<std::vector<cpptype>> const& v)                  \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
                                                                                                   \
    /* use a numpy view with the shared pointer tied up in a lifeline object (note: this */        \
    /* is just a demonstrator; alternatives are still being considered) */                         \
    npy_intp dims[] = {static_cast<npy_intp>(v->size())};                                          \
                                                                                                   \
    PyObject* np_view = PyArray_SimpleNewFromData(1,        /* 1-D array */                        \
                                                  dims,     /* dimension sizes */                  \
                                                  nptype,   /* numpy C type */                     \
                                                  v->data() /* raw buffer */                       \
    );                                                                                             \
                                                                                                   \
    if (!np_view)                                                                                  \
      return PyObjectPtr();                                                                        \
                                                                                                   \
    /* make the data read-only by not making it writable */                                        \
    PyArray_CLEARFLAGS((PyArrayObject*)np_view, NPY_ARRAY_WRITEABLE);                              \
                                                                                                   \
    /* create a lifeline object to tie this array and the original handle together; note */        \
    /* that the callback code needs to pick the data member out of the lifeline object, */         \
    /* when passing it to the registered Python function */                                        \
    py_lifeline_t* pyll =                                                                          \
      (py_lifeline_t*)PhlexLifeline_Type.tp_new(&PhlexLifeline_Type, nullptr, nullptr);            \
    new (&pyll->m_source) std::shared_ptr<void>(v);                                                \
    pyll->m_view = np_view; /* steals reference */                                                 \
                                                                                                   \
    return PyObjectPtr((PyObject*)pyll, PyObjectDeleter());                                        \
  }

  // VECTOR_CONVERTER(vint, int, NPY_INT)
  static PyObjectPtr vint_to_py(std::shared_ptr<std::vector<int>> const& v)
  {
    PyGILRAII gil;
    if (!v)
      return PyObjectPtr();
    PyObject* list = PyList_New(v->size());
    if (!list) {
      PyErr_Print();
      return PyObjectPtr();
    }
    for (size_t i = 0; i < v->size(); ++i) {
      PyObject* item = PyLong_FromLong((*v)[i]);
      if (!item) {
        PyErr_Print();
        Py_DECREF(list);
        return PyObjectPtr();
      }
      PyList_SET_ITEM(list, i, item);
    }
    return PyObjectPtr(list, PyObjectDeleter());
  }
  // VECTOR_CONVERTER(vuint, unsigned int, NPY_UINT)
  static PyObjectPtr vuint_to_py(std::shared_ptr<std::vector<unsigned int>> const& v)
  {
    PyGILRAII gil;
    if (!v)
      return PyObjectPtr();
    PyObject* list = PyList_New(v->size());
    if (!list) {
      PyErr_Print();
      return PyObjectPtr();
    }
    for (size_t i = 0; i < v->size(); ++i) {
      PyObject* item = PyLong_FromUnsignedLong((*v)[i]);
      if (!item) {
        PyErr_Print();
        Py_DECREF(list);
        return PyObjectPtr();
      }
      PyList_SET_ITEM(list, i, item);
    }
    return PyObjectPtr(list, PyObjectDeleter());
  }
  // VECTOR_CONVERTER(vlong, long, NPY_LONG)
  static PyObjectPtr vlong_to_py(std::shared_ptr<std::vector<long>> const& v)
  {
    PyGILRAII gil;
    if (!v)
      return PyObjectPtr();
    PyObject* list = PyList_New(v->size());
    if (!list) {
      PyErr_Print();
      return PyObjectPtr();
    }
    for (size_t i = 0; i < v->size(); ++i) {
      PyObject* item = PyLong_FromLong((*v)[i]);
      if (!item) {
        PyErr_Print();
        Py_DECREF(list);
        return PyObjectPtr();
      }
      PyList_SET_ITEM(list, i, item);
    }
    return PyObjectPtr(list, PyObjectDeleter());
  }
  // VECTOR_CONVERTER(vulong, unsigned long, NPY_ULONG)
  static PyObjectPtr vulong_to_py(std::shared_ptr<std::vector<unsigned long>> const& v)
  {
    PyGILRAII gil;
    if (!v)
      return PyObjectPtr();
    PyObject* list = PyList_New(v->size());
    if (!list) {
      PyErr_Print();
      return PyObjectPtr();
    }
    for (size_t i = 0; i < v->size(); ++i) {
      PyObject* item = PyLong_FromUnsignedLong((*v)[i]);
      if (!item) {
        PyErr_Print();
        Py_DECREF(list);
        return PyObjectPtr();
      }
      PyList_SET_ITEM(list, i, item);
    }
    return PyObjectPtr(list, PyObjectDeleter());
  }
  VECTOR_CONVERTER(vfloat, float, NPY_FLOAT)
  VECTOR_CONVERTER(vdouble, double, NPY_DOUBLE)

#define NUMPY_ARRAY_CONVERTER(name, cpptype, nptype)                                               \
  static std::shared_ptr<std::vector<cpptype>> py_to_##name(PyObjectPtr pyobj)                     \
  {                                                                                                \
    PyGILRAII gil;                                                                                 \
                                                                                                   \
    auto vec = std::make_shared<std::vector<cpptype>>();                                           \
                                                                                                   \
    /* TODO: because of unresolved ownership issues, copy the full array contents */               \
    if (!pyobj || !PyArray_Check(pyobj.get())) {                                                   \
      PyErr_Clear(); /* how to report an error? */                                                 \
      return vec;                                                                                  \
    }                                                                                              \
                                                                                                   \
    PyArrayObject* arr = (PyArrayObject*)pyobj.get();                                              \
                                                                                                   \
    /* TODO: flattening the array here seems to be the only workable solution */                   \
    npy_intp* dims = PyArray_DIMS(arr);                                                            \
    int nd = PyArray_NDIM(arr);                                                                    \
    size_t total = 1;                                                                              \
    for (int i = 0; i < nd; ++i)                                                                   \
      total *= static_cast<size_t>(dims[i]);                                                       \
                                                                                                   \
    /* copy the array info; note that this assumes C continuity */                                 \
    cpptype* raw = static_cast<cpptype*>(PyArray_DATA(arr));                                       \
    vec->reserve(total);                                                                           \
    vec->insert(vec->end(), raw, raw + total);                                                     \
                                                                                                   \
    return vec;                                                                                    \
  }

  // NUMPY_ARRAY_CONVERTER(vint, int, NPY_INT)
  // NUMPY_ARRAY_CONVERTER(vuint, unsigned int, NPY_UINT)
  // NUMPY_ARRAY_CONVERTER(vlong, long, NPY_LONG)
  // NUMPY_ARRAY_CONVERTER(vulong, unsigned long, NPY_ULONG)
  // NUMPY_ARRAY_CONVERTER(vfloat, float, NPY_FLOAT)
  // NUMPY_ARRAY_CONVERTER(vdouble, double, NPY_DOUBLE)

  // NUMPY_ARRAY_CONVERTER(vint, int, NPY_INT)
  static std::shared_ptr<std::vector<int>> py_to_vint(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<int>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          long val = PyLong_AsLong(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back((int)val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        int* raw = static_cast<int*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }
  // NUMPY_ARRAY_CONVERTER(vuint, unsigned int, NPY_UINT)
  static std::shared_ptr<std::vector<unsigned int>> py_to_vuint(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<unsigned int>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          unsigned long val = PyLong_AsUnsignedLong(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back((unsigned int)val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        unsigned int* raw = static_cast<unsigned int*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }
  // NUMPY_ARRAY_CONVERTER(vlong, long, NPY_LONG)
  static std::shared_ptr<std::vector<long>> py_to_vlong(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<long>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          long val = PyLong_AsLong(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back(val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        long* raw = static_cast<long*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }
  // NUMPY_ARRAY_CONVERTER(vulong, unsigned long, NPY_ULONG)
  static std::shared_ptr<std::vector<unsigned long>> py_to_vulong(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<unsigned long>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          unsigned long val = PyLong_AsUnsignedLong(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back(val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        unsigned long* raw = static_cast<unsigned long*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }
  // NUMPY_ARRAY_CONVERTER(vfloat, float, NPY_FLOAT)
  static std::shared_ptr<std::vector<float>> py_to_vfloat(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<float>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          double val = PyFloat_AsDouble(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back((float)val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        float* raw = static_cast<float*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }
  // NUMPY_ARRAY_CONVERTER(vdouble, double, NPY_DOUBLE)
  static std::shared_ptr<std::vector<double>> py_to_vdouble(PyObjectPtr pyobj)
  {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<double>>();
    PyObject* obj = pyobj.get();

    if (obj) {
      if (PyList_Check(obj)) {
        size_t size = PyList_Size(obj);
        vec->reserve(size);
        for (size_t i = 0; i < size; ++i) {
          PyObject* item = PyList_GetItem(obj, i);
          if (!item) {
            PyErr_Print();
            break;
          }
          double val = PyFloat_AsDouble(item);
          if (PyErr_Occurred()) {
            PyErr_Print();
            break;
          }
          vec->push_back(val);
        }
      } else if (PyArray_Check(obj)) {
        PyArrayObject* arr = (PyArrayObject*)obj;
        npy_intp* dims = PyArray_DIMS(arr);
        int nd = PyArray_NDIM(arr);
        size_t total = 1;
        for (int i = 0; i < nd; ++i)
          total *= static_cast<size_t>(dims[i]);

        double* raw = static_cast<double*>(PyArray_DATA(arr));
        vec->reserve(total);
        vec->insert(vec->end(), raw, raw + total);
      }
    }
    return vec;
  }

} // unnamed namespace

#define INSERT_INPUT_CONVERTER(name, alg, inp)                                                     \
  mod->ph_module->transform("py" #name "_" + inp + "_" + alg, name##_to_py, concurrency::serial)   \
    .input_family(product_query{product_specification::create(inp), LAYER})                        \
    .output_products(alg + "_" + inp + "py")

#define INSERT_OUTPUT_CONVERTER(name, alg, outp)                                                   \
  mod->ph_module->transform(#name "py_" + outp + "_" + alg, py_to_##name, concurrency::serial)     \
    .input_family(product_query{product_specification::create("py" + outp + "_" + alg), LAYER})    \
    .output_products(outp)

static PyObject* parse_args(PyObject* args,
                            PyObject* kwds,
                            std::string& functor_name,
                            std::vector<std::string>& input_labels,
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
  } else
    Py_INCREF(pyname);
  functor_name = PyUnicode_AsUTF8(pyname);
  Py_DECREF(pyname);

  if (!input) {
    PyErr_SetString(PyExc_TypeError, "an input is required");
    return nullptr;
  }

  if (!PySequence_Check(input) || (output && !PySequence_Check(output))) {
    PyErr_SetString(PyExc_TypeError, "input and output need to be sequences");
    return nullptr;
  }

  // convert input and output declarations, to be able to pass them to Phlex
  input_labels = cseq(input);
  output_labels = cseq(output);
  if (output_labels.size() > 1) {
    PyErr_SetString(PyExc_TypeError, "only a single output supported");
    return nullptr;
  }

  // retrieve C++ (matching) types from annotations
  input_types.reserve(input_labels.size());

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

  if (annot && PyDict_Check(annot) && PyDict_Size(annot)) {
    PyObject* ret = PyDict_GetItemString(annot, "return");
    if (ret)
      output_types.push_back(annotation_as_text(ret));

    // dictionary is ordered with return last if provide (note: the keys here
    // could be used as input labels, instead of the ones from the configuration,
    // but that is probably not practical in actual use, so they are ignored)

    // Re-implementing robust annotation extraction
    PyObject *key, *val;
    Py_ssize_t pos = 0;

    while (PyDict_Next(annot, &pos, &key, &val)) {
      // Skip 'return' annotation as it is handled separately
      if (PyUnicode_Check(key) && PyUnicode_CompareWithASCIIString(key, "return") == 0) {
        continue;
      }
      input_types.push_back(annotation_as_text(val));
    }
  }
  Py_XDECREF(annot);

  // ignore None as Python's conventional "void" return, which is meaningless in C++
  if (output_types.size() == 1 && output_types[0] == "None")
    output_types.clear();

  // if annotations were correct (and correctly parsed), there should be as many
  // input types as input labels
  if (input_types.size() != input_labels.size()) {
    PyErr_Format(PyExc_TypeError,
                 "number of inputs (%d; %s) does not match number of annotation types (%d; %s)",
                 input_labels.size(),
                 stringify(input_labels).c_str(),
                 input_types.size(),
                 stringify(input_types).c_str());
    return nullptr;
  }

  // special case of Phlex Variant wrapper
  PyObject* wrapped_callable = PyObject_GetAttrString(callable, "phlex_callable");
  if (wrapped_callable) {
    callable = wrapped_callable;
  } else {
    PyErr_Clear();
    Py_INCREF(callable);
  }

  // no common errors detected; actual registration may have more checks
  return callable;
}

static bool insert_input_converters(py_phlex_module* mod,
                                    std::string const& cname, // TODO: shared_ptr<PyObject>
                                    std::vector<std::string> const& input_labels,
                                    std::vector<std::string> const& input_types)
{
  // insert input converter nodes into the graph
  for (size_t i = 0; i < (size_t)input_labels.size(); ++i) {
    // TODO: this seems overly verbose and inefficient, but the function needs
    // to be properly types, so every option is made explicit
    auto const& inp = input_labels[i];
    auto const& inp_type = input_types[i];

    if (inp_type == "bool")
      INSERT_INPUT_CONVERTER(bool, cname, inp);
    else if (inp_type == "int")
      INSERT_INPUT_CONVERTER(int, cname, inp);
    else if (inp_type == "unsigned int")
      INSERT_INPUT_CONVERTER(uint, cname, inp);
    else if (inp_type == "long")
      INSERT_INPUT_CONVERTER(long, cname, inp);
    else if (inp_type == "unsigned long")
      INSERT_INPUT_CONVERTER(ulong, cname, inp);
    else if (inp_type == "float")
      INSERT_INPUT_CONVERTER(float, cname, inp);
    else if (inp_type == "double")
      INSERT_INPUT_CONVERTER(double, cname, inp);
    else if (inp_type.compare(0, 13, "numpy.ndarray") == 0) {
      // TODO: these are hard-coded std::vector <-> numpy array mappings, which is
      // way too simplistic for real use. It only exists for demonstration purposes,
      // until we have an IDL
      auto pos = inp_type.rfind("numpy.dtype");
      if (pos == std::string::npos) {
        PyErr_Format(
          PyExc_TypeError, "could not determine dtype of input type \"%s\"", inp_type.c_str());
        return false;
      }

      std::string suffix = inp_type.substr(pos);
      std::string py_out = cname + "_" + inp + "py";

      if (suffix.find("uint32]]") != std::string::npos) {
        mod->ph_module->transform("pyvuint_" + inp + "_" + cname, vuint_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else if (suffix.find("int32]]") != std::string::npos) {
        mod->ph_module->transform("pyvint_" + inp + "_" + cname, vint_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else if (suffix.find("uint64]]") != std::string::npos) { // id.
        mod->ph_module
          ->transform("pyvulong_" + inp + "_" + cname, vulong_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else if (suffix.find("int64]]") != std::string::npos) { // need not be true
        mod->ph_module->transform("pyvlong_" + inp + "_" + cname, vlong_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else if (suffix.find("float32]]") != std::string::npos) {
        mod->ph_module
          ->transform("pyvfloat_" + inp + "_" + cname, vfloat_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else if (suffix.find("float64]]") != std::string::npos) {
        mod->ph_module
          ->transform("pyvdouble_" + inp + "_" + cname, vdouble_to_py, concurrency::serial)
          .input_family(product_query{product_specification::create(inp), LAYER})
          .output_products(py_out);
      } else {
        PyErr_Format(PyExc_TypeError, "unsupported array input type \"%s\"", inp_type.c_str());
        return false;
      }
    }
    else if (inp_type == "list[int]") {
      std::string py_out = cname + "_" + inp + "py";
      mod->ph_module->transform("pyvint_" + inp + "_" + cname, vint_to_py, concurrency::serial)
        .input_family(product_query{product_specification::create(inp), LAYER})
        .output_products(py_out);
    } else if (inp_type == "list[float]") {
      std::string py_out = cname + "_" + inp + "py";
      mod->ph_module->transform("pyvfloat_" + inp + "_" + cname, vfloat_to_py, concurrency::serial)
        .input_family(product_query{product_specification::create(inp), LAYER})
        .output_products(py_out);
    } else if (inp_type == "list[double]" || inp_type == "list['double']") {
      std::string py_out = cname + "_" + inp + "py";
      mod->ph_module
        ->transform("pyvdouble_" + inp + "_" + cname, vdouble_to_py, concurrency::serial)
        .input_family(product_query{product_specification::create(inp), LAYER})
        .output_products(py_out);
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
  std::vector<std::string> input_labels, input_types, output_labels, output_types;
  PyObject* callable =
    parse_args(args, kwds, cname, input_labels, input_types, output_labels, output_types);
  if (!callable)
    return nullptr; // error already set

  if (output_types.empty()) {
    PyErr_Format(PyExc_TypeError, "a transform should have an output type");
    return nullptr;
  }

  // TODO: only support single output type for now, as there has to be a mapping
  // onto a std::tuple otherwise, which is a typed object, thus complicating the
  // template instantiation
  std::string output = output_labels[0];
  std::string output_type = output_types[0];

  if (!insert_input_converters(mod, cname, input_labels, input_types))
    return nullptr; // error already set

  // register Python transform
  std::string py_out = "py" + output + "_" + cname;
  if (input_labels.size() == 1) {
    auto* pyc = new py_callback_1{callable}; // TODO: leaks, but has program lifetime
    mod->ph_module->transform(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER})
      .output_products(py_out);
  } else if (input_labels.size() == 2) {
    auto* pyc = new py_callback_2{callable};
    mod->ph_module->transform(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[1] + "py"), LAYER})
      .output_products(py_out);
  } else if (input_labels.size() == 3) {
    auto* pyc = new py_callback_3{callable};
    mod->ph_module->transform(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[1] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[2] + "py"), LAYER})
      .output_products(py_out);
  } else {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    return nullptr;
  }

  // insert output converter node into the graph (TODO: same as above; these
  // are explicit b/c of the templates only)
  if (output_type == "bool")
    INSERT_OUTPUT_CONVERTER(bool, cname, output);
  else if (output_type == "int")
    INSERT_OUTPUT_CONVERTER(int, cname, output);
  else if (output_type == "unsigned int")
    INSERT_OUTPUT_CONVERTER(uint, cname, output);
  else if (output_type == "long")
    INSERT_OUTPUT_CONVERTER(long, cname, output);
  else if (output_type == "unsigned long")
    INSERT_OUTPUT_CONVERTER(ulong, cname, output);
  else if (output_type == "float")
    INSERT_OUTPUT_CONVERTER(float, cname, output);
  else if (output_type == "double")
    INSERT_OUTPUT_CONVERTER(double, cname, output);
  else if (output_type.compare(0, 13, "numpy.ndarray") == 0) {
    // TODO: just like for input types, these are hard-coded, but should be handled by
    // an IDL instead.
    auto pos = output_type.rfind("numpy.dtype");
    if (pos == std::string::npos) {
      PyErr_Format(
        PyExc_TypeError, "could not determine dtype of input type \"%s\"", output_type.c_str());
      return nullptr;
    }

    pos += 18;

    auto py_in = "py" + output + "_" + cname;
    if (output_type.compare(pos, std::string::npos, "int32]]") == 0) {
      mod->ph_module->transform("pyvint_" + output + "_" + cname, py_to_vint, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else if (output_type.compare(pos, std::string::npos, "uint32]]") == 0) {
      mod->ph_module->transform("pyvuint_" + output + "_" + cname, py_to_vuint, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else if (output_type.compare(pos, std::string::npos, "int64]]") == 0) { // need not be true
      mod->ph_module->transform("pyvlong_" + output + "_" + cname, py_to_vlong, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else if (output_type.compare(pos, std::string::npos, "uint64]]") == 0) { // id.
      mod->ph_module
        ->transform("pyvulong_" + output + "_" + cname, py_to_vulong, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else if (output_type.compare(pos, std::string::npos, "float32]]") == 0) {
      mod->ph_module
        ->transform("pyvfloat_" + output + "_" + cname, py_to_vfloat, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else if (output_type.compare(pos, std::string::npos, "float64]]") == 0) {
      mod->ph_module
        ->transform("pyvdouble_" + output + "_" + cname, py_to_vdouble, concurrency::serial)
        .input_family(product_query{product_specification::create(py_in), LAYER})
        .output_products(output);
    } else {
      PyErr_Format(PyExc_TypeError, "unsupported array output type \"%s\"", output_type.c_str());
      return nullptr;
    }
  }
  else if (output_type == "list[int]") {
    auto py_in = "py" + output + "_" + cname;
    mod->ph_module->transform("pyvint_" + output + "_" + cname, py_to_vint, concurrency::serial)
      .input_family(product_query{product_specification::create(py_in), LAYER})
      .output_products(output);
  } else if (output_type == "list[float]") {
    auto py_in = "py" + output + "_" + cname;
    mod->ph_module->transform("pyvfloat_" + output + "_" + cname, py_to_vfloat, concurrency::serial)
      .input_family(product_query{product_specification::create(py_in), LAYER})
      .output_products(output);
  } else if (output_type == "list[double]" || output_type == "list['double']") {
    auto py_in = "py" + output + "_" + cname;
    mod->ph_module
      ->transform("pyvdouble_" + output + "_" + cname, py_to_vdouble, concurrency::serial)
      .input_family(product_query{product_specification::create(py_in), LAYER})
      .output_products(output);
  } else {
    PyErr_Format(PyExc_TypeError, "unsupported output type \"%s\"", output_type.c_str());
    return nullptr;
  }

  Py_RETURN_NONE;
}

static PyObject* md_observe(py_phlex_module* mod, PyObject* args, PyObject* kwds)
{
  // Register a python observer by adding the necessary intermediate converter
  // nodes going from C++ to PyObject* and back.

  std::string cname;
  std::vector<std::string> input_labels, input_types, output_labels, output_types;
  PyObject* callable =
    parse_args(args, kwds, cname, input_labels, input_types, output_labels, output_types);
  if (!callable)
    return nullptr; // error already set

  if (!output_types.empty()) {
    PyErr_Format(PyExc_TypeError, "an observer should not have an output type");
    return nullptr;
  }

  if (!insert_input_converters(mod, cname, input_labels, input_types))
    return nullptr; // error already set

  // register Python observer
  if (input_labels.size() == 1) {
    auto* pyc = new py_callback_1v{callable}; // id.
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER});
  } else if (input_labels.size() == 2) {
    auto* pyc = new py_callback_2v{callable};
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[1] + "py"), LAYER});
  } else if (input_labels.size() == 3) {
    auto* pyc = new py_callback_3v{callable};
    mod->ph_module->observe(cname, *pyc, concurrency::serial)
      .input_family(
        product_query{product_specification::create(cname + "_" + input_labels[0] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[1] + "py"), LAYER},
        product_query{product_specification::create(cname + "_" + input_labels[2] + "py"), LAYER});
  } else {
    PyErr_SetString(PyExc_TypeError, "unsupported number of inputs");
    return nullptr;
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

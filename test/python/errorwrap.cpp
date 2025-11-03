#include "wrap.hpp"

#include <exception>
#include <string>

using namespace phlex::experimental;

void phlex::experimental::throw_runtime_error_from_py_error(bool check_error)
{
  PyGILRAII g;

  if (check_error) {
    if (!PyErr_Occurred())
      return
  }

  std::string msg;

#if PY_VERSION_HEX < 0x30c000000
  PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
  PyErr_Fetch(&type, &value, &traceback);
  if (value) {
    PyObject* pymsg = PyObject_Str(value);
    msg = PyUnicode_AsUTF8(pymsg);
    Py_DECREF(pymsg);
  } else {
    msg = "unknown Python error occurred";
  }
  Py_XDECREF(traceback);
  Py_XDECREF(value);
  Py_XDECREF(type);
#else
  PyObject* exc = PyErr_GetRaisedException();
  if (exc) {
    PyObject* pymsg = PyObject_Str(exc);
    msg = PyUnicode_AsString(pymsg);
    Py_DECREF(pymsg);
    Py_DECREF(exc);
  }
#endif

  throw std::runtime_error(msg.c_str());
}

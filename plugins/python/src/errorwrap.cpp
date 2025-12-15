#include "wrap.hpp"

#include <string>

using namespace phlex::experimental;

static bool format_traceback(std::string& msg,
#if PY_VERSION_HEX < 0x30c000000
                             PyObject* type,
                             PyObject* value,
                             PyObject* traceback)
#else
                             PyObject* exception)
#endif
{
  PyObject* tbmod = PyImport_ImportModule("traceback");
  PyObject* format_exception = PyObject_GetAttrString(tbmod, "format_exception");
  Py_DECREF(tbmod);

  PyObject* formatted_tb = PyObject_CallFunctionObjArgs(
#if PY_VERSION_HEX < 0x30c000000
    format_exception, type, value, traceback, nullptr);
#else
    format_exception, exception, nullptr);
#endif
  Py_DECREF(format_exception);

  if (!formatted_tb) {
    PyErr_Clear();
    return false;
  }

  PyObject* py_msg_empty = PyUnicode_FromString("");
  PyObject* py_msg = PyUnicode_Join(py_msg_empty, formatted_tb);
  Py_DECREF(py_msg_empty);
  Py_DECREF(formatted_tb);

  if (!py_msg) {
    PyErr_Clear();
    return false;
  }

  char const* c_msg = PyUnicode_AsUTF8(py_msg);
  if (c_msg) {
    msg = c_msg;
    Py_DECREF(py_msg);
    return true;
  }

  PyErr_Clear();
  Py_DECREF(py_msg);
  return false;
}

bool phlex::experimental::msg_from_py_error(std::string& msg, bool check_error)
{
  PyGILRAII g;

  if (check_error) {
    if (!PyErr_Occurred())
      return false;
  }

#if PY_VERSION_HEX < 0x30c000000
  PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
  PyErr_Fetch(&type, &value, &traceback);
  if (value) {
    bool tb_ok = format_traceback(msg, type, value, traceback);
    if (!tb_ok) {
      PyObject* pymsg = PyObject_Str(value);
      msg = PyUnicode_AsUTF8(pymsg);
      Py_DECREF(pymsg);
    }
  } else {
    msg = "unknown Python error occurred";
  }
  Py_XDECREF(traceback);
  Py_XDECREF(value);
  Py_XDECREF(type);
#else
  PyObject* exc = PyErr_GetRaisedException();
  if (exc) {
    bool tb_ok = format_traceback(msg, exc);
    if (!tb_ok) {
      PyObject* pymsg = PyObject_Str(exc);
      msg = PyUnicode_AsUTF8(pymsg);
      Py_DECREF(pymsg);
    }
    Py_DECREF(exc);
  }
#endif

  return true;
}

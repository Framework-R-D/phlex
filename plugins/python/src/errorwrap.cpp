#include "wrap.hpp"

#include <string>

// This code has excluded several error checking paths from code coverage,
// because the conditions to create the errors (trace formatting problems)
// are rare and too hard to recreate in a test, while the resolution (fall
// back to a generic error messsage) is rather straightforward and thus
// does not need testing.

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

  // LCOV_EXCL_START
  if (!formatted_tb) {
    PyErr_Clear();
    return false;
  }
  // LCOV_EXCL_STOP

  PyObject* py_msg_empty = PyUnicode_FromString("");
  PyObject* py_msg = PyUnicode_Join(py_msg_empty, formatted_tb);
  Py_DECREF(py_msg_empty);
  Py_DECREF(formatted_tb);

  // LCOV_EXCL_START
  if (!py_msg) {
    PyErr_Clear();
    return false;
  }
  // LCOV_EXCL_STOP

  char const* c_msg = PyUnicode_AsUTF8(py_msg);
  if (c_msg) {
    msg = c_msg;
    Py_DECREF(py_msg);
    return true;
  }

  // LCOV_EXCL_START
  PyErr_Clear();
  Py_DECREF(py_msg);
  return false;
  // LCOV_EXCL_STOP
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
    // LCOV_EXCL_START
    if (!tb_ok) {
      PyObject* pymsg = PyObject_Str(value);
      msg = PyUnicode_AsUTF8(pymsg);
      Py_DECREF(pymsg);
    }
    // LCOV_EXCL_STOP
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

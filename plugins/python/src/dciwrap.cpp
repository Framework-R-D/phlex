#include "phlex/model/data_cell_index.hpp"
#include "wrap.hpp"

using namespace phlex::experimental;
using namespace phlex;

// Provide selected (for now) access to Phlex's data_cell_index instances.
// clang-format off
namespace phlex::experimental {
  struct py_data_cell_index {
    PyObject_HEAD
    data_cell_index const* ph_dci;
  };
}
// clang-format on

PyObject* phlex::experimental::wrap_dci(data_cell_index const& dci)
{
  py_data_cell_index* pydci = PyObject_New(py_data_cell_index, &PhlexDataCellIndex_Type);
  pydci->ph_dci = &dci;

  return reinterpret_cast<PyObject*>(pydci);
}

// simple forwarding methods
static PyObject* dci_number(py_data_cell_index* pydci)
{
  return PyLong_FromLong((long)pydci->ph_dci->number());
}

// PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static PyMethodDef dci_methods[] = {
  {"number", reinterpret_cast<PyCFunction>(dci_number), METH_NOARGS, "index number"},
  {nullptr, nullptr, 0, nullptr}};

// clang-format off
// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::PhlexDataCellIndex_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "pyphlex.data_cell_index",                         // tp_name
  sizeof(py_data_cell_index),                        // tp_basicsize
  0,                                                 // tp_itemsize
  nullptr,                                           // tp_dealloc
  0,                                                 // tp_vectorcall_offset / tp_print
  nullptr,                                           // tp_getattr
  nullptr,                                           // tp_setattr
  nullptr,                                           // tp_as_async / tp_compare
  nullptr,                                           // tp_repr
  nullptr,                                           // tp_as_number
  nullptr,                                           // tp_as_sequence
  nullptr,                                           // tp_as_mapping
  nullptr,                                           // tp_hash
  nullptr,                                           // tp_call
  nullptr,                                           // tp_str
  nullptr,                                           // tp_getattro
  nullptr,                                           // tp_setattro
  nullptr,                                           // tp_as_buffer
  Py_TPFLAGS_DEFAULT,                                // tp_flags
  "phlex data_cell_index",                           // tp_doc
  nullptr,                                           // tp_traverse
  nullptr,                                           // tp_clear
  nullptr,                                           // tp_richcompare
  0,                                                 // tp_weaklistoffset
  nullptr,                                           // tp_iter
  nullptr,                                           // tp_iternext
  dci_methods,                                       // tp_methods
  nullptr,                                           // tp_members
  nullptr,                                           // tp_getset
  nullptr,                                           // tp_base
  nullptr,                                           // tp_dict
  nullptr,                                           // tp_descr_get
  nullptr,                                           // tp_descr_set
  0,                                                 // tp_dictoffset
  nullptr,                                           // tp_init
  nullptr,                                           // tp_alloc
  nullptr,                                           // tp_new
  nullptr,                                           // tp_free
  nullptr,                                           // tp_is_gc
  nullptr,                                           // tp_bases
  nullptr,                                           // tp_mro
  nullptr,                                           // tp_cache
  nullptr,                                           // tp_subclasses
  nullptr                                            // tp_weaklist
#if PY_VERSION_HEX >= 0x02030000
  , nullptr                                          // tp_del
#endif
#if PY_VERSION_HEX >= 0x02060000
  , 0                                                // tp_version_tag
#endif
#if PY_VERSION_HEX >= 0x03040000
  , nullptr                                          // tp_finalize
#endif
#if PY_VERSION_HEX >= 0x03080000
  , nullptr                                          // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
  , 0                                                // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
  , 0                                                // tp_versions_used
#endif
};
// clang-format on

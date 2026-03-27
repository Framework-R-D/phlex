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

  return (PyObject*)pydci;
}

// simple forwarding methods
static PyObject* dci_number(py_data_cell_index* pydci)
{
  return PyLong_FromLong((long)pydci->ph_dci->number());
}

static PyMethodDef dci_methods[] = {
  {(char*)"number", (PyCFunction)dci_number, METH_NOARGS, (char*)"index number"},
  {(char*)nullptr, nullptr, 0, nullptr}};

// clang-format off
PyTypeObject phlex::experimental::PhlexDataCellIndex_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  (char*) "pyphlex.data_cell_index",                 // tp_name
  sizeof(py_data_cell_index),                        // tp_basicsize
  0,                                                 // tp_itemsize
  0,                                                 // tp_dealloc
  0,                                                 // tp_vectorcall_offset / tp_print
  0,                                                 // tp_getattr
  0,                                                 // tp_setattr
  0,                                                 // tp_as_async / tp_compare
  0,                                                 // tp_repr
  0,                                                 // tp_as_number
  0,                                                 // tp_as_sequence
  0,                                                 // tp_as_mapping
  0,                                                 // tp_hash
  0,                                                 // tp_call
  0,                                                 // tp_str
  0,                                                 // tp_getattro
  0,                                                 // tp_setattro
  0,                                                 // tp_as_buffer
  Py_TPFLAGS_DEFAULT,                                // tp_flags
  (char*)"phlex data_cell_index",                    // tp_doc
  0,                                                 // tp_traverse
  0,                                                 // tp_clear
  0,                                                 // tp_richcompare
  0,                                                 // tp_weaklistoffset
  0,                                                 // tp_iter
  0,                                                 // tp_iternext
  dci_methods,                                       // tp_methods
  0,                                                 // tp_members
  0,                                                 // tp_getset
  0,                                                 // tp_base
  0,                                                 // tp_dict
  0,                                                 // tp_descr_get
  0,                                                 // tp_descr_set
  0,                                                 // tp_dictoffset
  0,                                                 // tp_init
  0,                                                 // tp_alloc
  0,                                                 // tp_new
  0,                                                 // tp_free
  0,                                                 // tp_is_gc
  0,                                                 // tp_bases
  0,                                                 // tp_mro
  0,                                                 // tp_cache
  0,                                                 // tp_subclasses
  0                                                  // tp_weaklist
#if PY_VERSION_HEX >= 0x02030000
  , 0                                                // tp_del
#endif
#if PY_VERSION_HEX >= 0x02060000
  , 0                                                // tp_version_tag
#endif
#if PY_VERSION_HEX >= 0x03040000
  , 0                                                // tp_finalize
#endif
#if PY_VERSION_HEX >= 0x03080000
  , 0                                                // tp_vectorcall
#endif
#if PY_VERSION_HEX >= 0x030c0000
  , 0                                                // tp_watched
#endif
#if PY_VERSION_HEX >= 0x030d0000
  , 0                                                // tp_versions_used
#endif
};
// clang-format on

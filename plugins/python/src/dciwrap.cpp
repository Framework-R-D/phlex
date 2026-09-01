#include "phlex/model/data_cell_index.hpp"
#include "wrap.hpp"

#include <array>

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
  py_data_cell_index* pydci = PyObject_New(py_data_cell_index, &phlex_data_cell_index_type);
  pydci->ph_dci = &dci;

  return reinterpret_cast<PyObject*>(pydci);
}

// simple forwarding methods
static PyObject* dci_number(py_data_cell_index* pydci)
{
  return PyLong_FromLong(static_cast<long>(pydci->ph_dci->number()));
}

// PyMethodDef arrays must be non-const; tp_methods in PyTypeObject takes a non-const pointer.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<PyMethodDef, 2> dci_methods{
  {{.ml_name = "number",
    .ml_meth = reinterpret_cast<PyCFunction>(dci_number),
    .ml_flags = METH_NOARGS,
    .ml_doc = "index number"},
   {.ml_name = nullptr, .ml_meth = nullptr, .ml_flags = 0, .ml_doc = nullptr}}};

// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::phlex_data_cell_index_type = {
  // clang-format off
  .ob_base = PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "pyphlex.data_cell_index",
  // clang-format on
  .tp_basicsize = sizeof(py_data_cell_index),
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
  .tp_doc = "phlex data_cell_index",
  .tp_traverse = nullptr,
  .tp_clear = nullptr,
  .tp_richcompare = nullptr,
  .tp_weaklistoffset = 0,
  .tp_iter = nullptr,
  .tp_iternext = nullptr,
  .tp_methods = dci_methods.data(),
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

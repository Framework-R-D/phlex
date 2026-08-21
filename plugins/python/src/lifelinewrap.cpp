#include <memory>
#include <string>

#include "wrap.hpp"

using namespace phlex::experimental;

static py_lifeline_t* ll_new(PyTypeObject* pytype, PyObject*, PyObject*)
{
  auto* pyobj = reinterpret_cast<py_lifeline_t*>(pytype->tp_alloc(pytype, 0));
  if (pyobj) {
    pyobj->m_view = nullptr;
    new (&pyobj->m_source) std::shared_ptr<void>{};
  } else {
    PyErr_Print();
  }
  return pyobj;
}

static int ll_traverse(py_lifeline_t* pyobj, visitproc visit, void* args)
{
  if (pyobj->m_view) {
    visit(pyobj->m_view, args);
  }
  return 0;
}

static int ll_clear(py_lifeline_t* pyobj)
{
  Py_CLEAR(pyobj->m_view);
  return 0;
}

static void ll_dealloc(py_lifeline_t* pyobj)
{
  // This type participates in GC; untrack before clearing references so the
  // collector does not traverse a partially torn-down object during dealloc.
  PyObject_GC_UnTrack(pyobj);
  Py_CLEAR(pyobj->m_view);
  using generic_shared_t = std::shared_ptr<void>;
  pyobj->m_source.~generic_shared_t();
  // Use tp_free to pair with tp_alloc for GC-tracked Python objects.
  Py_TYPE(pyobj)->tp_free(reinterpret_cast<PyObject*>(pyobj));
}

// PyType_Ready() modifies PyTypeObject in-place; the Python C API requires non-const.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
PyTypeObject phlex::experimental::PhlexLifeline_Type = {
  // clang-format off
  .ob_base = PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "pyphlex.lifeline",
  // clang-format on
  .tp_basicsize = sizeof(py_lifeline_t),
  .tp_itemsize = 0,
  .tp_dealloc = reinterpret_cast<destructor>(ll_dealloc),
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
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
  .tp_doc = "internal",
  .tp_traverse = reinterpret_cast<traverseproc>(ll_traverse),
  .tp_clear = reinterpret_cast<inquiry>(ll_clear),
  .tp_richcompare = nullptr,
  .tp_weaklistoffset = 0,
  .tp_iter = nullptr,
  .tp_iternext = nullptr,
  .tp_methods = nullptr,
  .tp_members = nullptr,
  .tp_getset = nullptr,
  .tp_base = nullptr,
  .tp_dict = nullptr,
  .tp_descr_get = nullptr,
  .tp_descr_set = nullptr,
  .tp_dictoffset = 0,
  .tp_init = nullptr,
  .tp_alloc = nullptr,
  .tp_new = reinterpret_cast<newfunc>(ll_new),
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

#include <memory>
#include <string>

#include "wrap.hpp"

using namespace phlex::experimental;

static py_lifeline_t* ll_new(PyTypeObject* pytype, PyObject*, PyObject*)
{
  py_lifeline_t* pyobj = (py_lifeline_t*)pytype->tp_alloc(pytype, 0);
  if (!pyobj)
    PyErr_Print();
  pyobj->m_view = nullptr;
  new (&pyobj->m_source) std::shared_ptr<void>{};

  return pyobj;
}

static int ll_traverse(py_lifeline_t* pyobj, visitproc visit, void* args)
{
  if (pyobj->m_view)
    visit(pyobj->m_view, args);
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
  typedef std::shared_ptr<void> generic_shared_t;
  pyobj->m_source.~generic_shared_t();
  // Use tp_free to pair with tp_alloc for GC-tracked Python objects.
  Py_TYPE(pyobj)->tp_free((PyObject*)pyobj);
}

// clang-format off
PyTypeObject phlex::experimental::PhlexLifeline_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  (char*) "pyphlex.lifeline",                        // tp_name
  sizeof(py_lifeline_t),                             // tp_basicsize
  0,                                                 // tp_itemsize
  (destructor)ll_dealloc,                            // tp_dealloc
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,           // tp_flags
  (char*)"internal",                                 // tp_doc
  (traverseproc)ll_traverse,                         // tp_traverse
  (inquiry)ll_clear,                                 // tp_clear
  0,                                                 // tp_richcompare
  0,                                                 // tp_weaklistoffset
  0,                                                 // tp_iter
  0,                                                 // tp_iternext
  0,                                                 // tp_methods
  0,                                                 // tp_members
  0,                                                 // tp_getset
  0,                                                 // tp_base
  0,                                                 // tp_dict
  0,                                                 // tp_descr_get
  0,                                                 // tp_descr_set
  0,                                                 // tp_dictoffset
  0,                                                 // tp_init
  0,                                                 // tp_alloc
  (newfunc)ll_new,                                   // tp_new
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

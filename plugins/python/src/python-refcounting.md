# Python Reference Counting in the Phlex Python Plugin

## Overview

Phlex's Python plugin bridges C++ and Python through `intptr_t` values
that represent `PyObject*` pointers. These values flow through the
framework's `declared_transform` nodes, which cache their outputs for
reuse when duplicate hashes appear. This document describes the
reference counting discipline used to avoid both leaks and crashes.

## Architecture

A typical Python transform pipeline looks like this:

```text
Provider → [input converter] → [py_callback] → [output converter] → Observer
             (C++ → PyObject)    (Py → Py)        (PyObject → C++)
```

Each `[…]` above is its own `declared_transform` node. Each node is
named with a combination of the type, the product label, and the
consumer algorithm, so every converter has a **unique** identity.

## The Caching Mechanism

`declared_transform` (see `phlex/core/declared_transform.hpp`) caches
its output product store by `data_cell_index::hash()`:

```cpp
if (stores_.insert(a, hash)) {
    // First time: call the transform, store result
    a->second = make_shared<product_store>(...);
} else {
    // Cache hit: reuse a->second without calling the transform
}
```

Each transform function is called **exactly once** per unique hash.
Duplicate events with the same hash reuse the cached product store
without re-invoking the transform. The cache entry is erased once
`done_with()` returns true (all events for that hash have been
processed).

## The One-to-One Model

Because each converter node has a unique name and each transform is
called once per unique hash, reference counting is **one-to-one**:

1. **Input converters** create a new `PyObject*` reference (refcnt=1).
2. **Consumers** (`py_callback` or output converters) `Py_DECREF` the
   reference after use (refcnt→0, freed).

No `XINCREF`/`XDECREF` wrapping is needed around the callback call.
Adding one would produce a net +1 leak per product.

### Input Converters (`_to_py`)

Each input converter creates a **new reference** that is stored in the
product store as an `intptr_t`. The framework treats `intptr_t` as an
opaque integer — it has no C++ destructor and cannot call `Py_DECREF`.
The consumer is responsible for releasing the reference.

```cpp
// BASIC_CONVERTER: creates new reference via Python C API
static intptr_t int_to_py(int a) {
    PyGILRAII gil;
    return (intptr_t)PyLong_FromLong(a);  // new reference, refcnt=1
}

// VECTOR_CONVERTER: creates new PhlexLifeline wrapping a numpy view
static intptr_t vint_to_py(shared_ptr<vector<int>> const& v) {
    // ... creates PyArrayObject + PhlexLifeline ...
    return (intptr_t)pyll;  // new reference, refcnt=1
}
```

### py_callback (`call` / `callv`)

`py_callback::call()` and `py_callback::callv()` receive `intptr_t`
args that are **owned references** created by the upstream input
converter, dedicated to this specific consumer. They:

1. Call `lifeline_transform()` to unwrap any `PhlexLifeline` objects
   (extracting the numpy view for the Python function) and store the
   results in a local array.
2. `Py_INCREF` each transformed arg to create a temporary owned
   reference — this protects borrowed `m_view` pointers from being
   invalidated during the Python call.
3. Pass the transformed args to `PyObject_CallFunctionObjArgs()`.
4. `Py_DECREF` each transformed arg (releasing the temporary refs).
5. Call `decref_all(args...)` to release the original input references
   (PhlexLifeline objects and basic PyObjects).

```cpp
template <typename... Args>
intptr_t call(Args... args) {
    PyGILRAII gil;
    PyObject* py_args[N] = {lifeline_transform(args)...};
    for (auto* p : py_args)
        Py_INCREF(p);
    PyObject* result = call_with_array(py_args, ...);
    // ... error handling ...
    for (auto* p : py_args)
        Py_DECREF(p);
    decref_all(args...);  // release input references
    return (intptr_t)result;  // new reference from Python call
}
```

Both `call()` and `callv()` use `lifeline_transform()` symmetrically.

### Output Converters (`py_to_*`)

Output converters receive the `intptr_t` from `py_callback`'s product
store — again, an **owned reference** created by the Python call,
dedicated to this specific output converter. They extract the C++
value, then `Py_DECREF` the input:

```cpp
// BASIC_CONVERTER py_to_*
static int py_to_int(intptr_t pyobj) {
    PyGILRAII gil;
    int i = (int)PyLong_AsLong((PyObject*)pyobj);
    // ... error handling ...
    Py_DECREF((PyObject*)pyobj);  // release the reference
    return i;
}

// NUMPY_ARRAY_CONVERTER py_to_*
static shared_ptr<vector<int>> py_to_vint(intptr_t pyobj) {
    PyGILRAII gil;
    auto vec = make_shared<vector<int>>();
    // ... copy data from PyArray or PyList ...
    Py_DECREF((PyObject*)pyobj);  // release the reference
    return vec;
}
```

## Reference Flow Diagram

```text
                    ┌──────────────┐
                    │   Provider   │  C++ value (int, float, vector<T>)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │ input conv.  │  Creates NEW PyObject* (refcnt=1)
                    │ (e.g. int_   │  Stored in product_store as intptr_t
                    │  to_py)      │  One per unique hash; consumer owns it
                    └──────┬───────┘
                           │  intptr_t (owned reference, refcnt=1)
                    ┌──────▼───────┐
                    │ py_callback  │  lifeline_transform() + INCREF to protect
                    │  ::call()    │  PyObject_CallFunctionObjArgs()
                    │              │  DECREF views, decref_all(args...) → freed
                    │              │  Returns result (NEW reference, refcnt=1)
                    └──────┬───────┘
                           │  intptr_t (owned reference, refcnt=1)
                    ┌──────▼───────┐
                    │ output conv. │  Reads PyObject* value
                    │ (e.g. py_to_ │  Py_DECREF → refcnt=0, freed
                    │  int)        │  Returns C++ value
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │   Observer   │  Uses C++ value
                    └──────────────┘
```

## Error Handling

### declared_transform Exception Safety

`declared_transform::stores_.insert()` creates an entry with a null
`product_store_ptr` before calling the transform function. If the
transform throws (e.g., because a converter encounters an error), the
null entry must be erased to prevent SEGFAULT on cache reuse. This is
handled by a `try/catch` block that erases the stale entry and
re-throws.

### VECTOR_CONVERTER Must Throw on Error

`VECTOR_CONVERTER` error paths throw `std::runtime_error` instead of
returning `(intptr_t)nullptr`. A null `intptr_t` passed to
`PyObject_CallFunctionObjArgs` would act as the argument-list sentinel,
silently truncating the argument list.

### lifeline_transform Returns a Borrowed Reference

`lifeline_transform()` unwraps `PhlexLifeline` objects to extract the
numpy array view (`m_view`). The returned pointer is a borrowed
reference from the lifeline object. Because this borrowed reference
could be invalidated by concurrent operations, `call()` and `callv()`
INCREF the transformed args before the Python call and DECREF them
after, creating temporary owned references that protect the views.

### ll_new Must Return nullptr on Allocation Failure

`ll_new` (in `lifelinewrap.cpp`) returns `nullptr` when `tp_alloc`
fails, rather than falling through to dereference the null pointer.

## Common Pitfalls

1. **Do not remove `decref_all` from `py_callback` or `Py_DECREF` from
   output converters.** These are the "consume" side of the one-to-one
   contract. Removing them leaks every converted object.

2. **Do not add bare `Py_XINCREF`/`Py_XDECREF` that change the net
   refcount.** The INCREF/DECREF in `call()`/`callv()` is balanced
   (net +0) and only protects borrowed views during the Python call.
   Adding unbalanced INCREF/DECREF introduces leaks.

3. **Do not return `(intptr_t)nullptr` from converters.** It acts as
   the `PyObject_CallFunctionObjArgs` sentinel. Throw an exception
   instead.

## Known Limitations

- `BASIC_CONVERTER`'s `_to_py` functions can return
  `(intptr_t)nullptr` if the underlying Python C API call fails (e.g.,
  `PyLong_FromLong` on OOM). This null would propagate through the
  product store and cause a crash in `decref_all`. This is an
  extremely rare edge case that has not been addressed yet.

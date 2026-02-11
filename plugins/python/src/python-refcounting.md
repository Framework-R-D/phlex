# Python Reference Counting Model for Phlex Transforms

## Overview

Phlex's Python plugin bridges C++ and Python through `intptr_t` values
that represent `PyObject*` pointers. These values flow through the
framework's `declared_transform` nodes, which cache their outputs for
reuse. This document describes the reference counting discipline
required to prevent use-after-free and memory leaks.

## Architecture

A typical Python transform pipeline looks like this:

```text
Provider → [input converter] → [Python callback] → [output converter] → Observer/Fold
             (C++ → PyObject)     (PyObject → PyObject)  (PyObject → C++)
```

Each `[…]` above is a `declared_transform` node. The framework caches
each node's output in a `stores_` map keyed by `data_cell_index::hash()`.
When multiple events share the same hash (e.g., all events within one
job), the cached product store is reused without re-running the
transform.

## The Caching Problem

The product store holds an `intptr_t` representing a `PyObject*`. This
is an opaque integer to the framework — it has no C++ destructor and no
way to call `Py_DECREF` on cleanup. This means:

1. **The cached reference is never freed** by the framework. This is an
   accepted, bounded leak (one reference per unique hash per converter).
2. **Consumers must not free the cached reference.** Any `Py_DECREF` on
   the cached `PyObject*` would free it, leaving a dangling pointer in
   the cache for subsequent events to access.

## Rules

### Rule 1: Input converters create new references

Input converters (`_to_py` functions in `BASIC_CONVERTER` and
`VECTOR_CONVERTER`) create a **new reference** (refcnt=1) that is
stored in the product store cache. The cache owns this reference.

```cpp
// BASIC_CONVERTER: creates new reference via Python C API
static intptr_t int_to_py(int a) {
    PyGILRAII gil;
    return (intptr_t)PyLong_FromLong(a);  // new reference, refcnt=1
}

// VECTOR_CONVERTER: creates new PhlexLifeline wrapping a numpy view
static intptr_t vint_to_py(std::shared_ptr<std::vector<int>> const& v) {
    // ... creates PyArrayObject and PhlexLifeline ...
    return (intptr_t)pyll;  // new reference, refcnt=1
}
```

### Rule 2: py_callback XINCREF/XDECREF around the Python call

`py_callback::call()` and `py_callback::callv()` receive `intptr_t`
args that are **borrowed references** from the upstream product store
cache. They must create temporary owned references for the duration of
the Python function call:

```cpp
template <typename... Args>
intptr_t call(Args... args) {
    PyGILRAII gil;

    // Create temporary owned references
    (Py_XINCREF((PyObject*)args), ...);

    PyObject* result = PyObject_CallFunctionObjArgs(
        m_callable, lifeline_transform(args)..., nullptr);

    // Release temporary references; cache references remain intact
    (Py_XDECREF((PyObject*)args), ...);

    return (intptr_t)result;  // new reference, owned by output cache
}
```

The `Py_XINCREF`/`Py_XDECREF` pair ensures that even if the Python
function or garbage collector decrements the object's reference count
during the call, the cached reference remains valid. The X variants
handle the case where an upstream converter returned null due to an
out-of-memory condition.

### Rule 3: Output converters must NOT Py_DECREF their input

Output converters (`py_to_*` functions in `BASIC_CONVERTER` and
`NUMPY_ARRAY_CONVERTER`) receive **borrowed references** from the
upstream product store cache. They must not call `Py_DECREF` on the
input:

```cpp
// BASIC_CONVERTER py_to_*: extracts C++ value, does NOT decref
static int py_to_int(intptr_t pyobj) {
    PyGILRAII gil;
    int i = (int)PyLong_AsLong((PyObject*)pyobj);
    // NO Py_DECREF — input is borrowed from cache
    return i;
}

// NUMPY_ARRAY_CONVERTER py_to_*: copies array data, does NOT decref
static std::shared_ptr<std::vector<int>> py_to_vint(intptr_t pyobj) {
    PyGILRAII gil;
    auto vec = std::make_shared<std::vector<int>>();
    // ... copy data from PyArray or PyList ...
    // NO Py_DECREF — input is borrowed from cache
    return vec;
}
```

### Rule 4: lifeline_transform returns a borrowed reference

`lifeline_transform()` unwraps `PhlexLifeline` objects to extract the
numpy array view. It returns a borrowed reference in both cases:

- If the arg is a `PhlexLifeline`, it returns `m_view` (a borrowed
  reference from the lifeline object, which stays alive because the
  caller holds a temporary INCREF on it per Rule 2).
- If the arg is a plain `PyObject`, it returns the arg itself (a
  borrowed reference from the product store cache, protected by the
  INCREF per Rule 2).

`lifeline_transform()` is used symmetrically in both `call()` and
`callv()`.

### Rule 5: VECTOR_CONVERTER must throw on error, never return null

`VECTOR_CONVERTER` error paths must throw `std::runtime_error` instead
of returning `(intptr_t)nullptr`. A null `intptr_t` passed to
`PyObject_CallFunctionObjArgs` acts as the argument-list sentinel,
silently truncating the argument list and causing the Python function to
receive fewer arguments than expected.

### Rule 6: declared_transform must erase stale cache entries

`declared_transform::stores_.insert()` creates an entry with a null
`product_store_ptr`. If the transform's `call()` throws before
assigning `a->second`, the null entry persists in the cache. Subsequent
events with the same hash hit the `else` branch and propagate the null
product store downstream, causing SEGFAULTs when downstream converters
attempt to use it.

Fix: wrap the transform body in `try/catch` and erase the stale entry
on exception:

```cpp
if (stores_.insert(a, hash)) {
    try {
        // ... compute and assign a->second ...
    } catch (...) {
        stores_.erase(a);
        throw;
    }
}
```

## Reference Flow Diagram

```text
                    ┌──────────────┐
                    │   Provider   │  C++ value (int, float, vector<T>)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │ input conv.  │  Creates NEW PyObject* reference (refcnt=1)
                    │ (e.g. int_   │  Stored in product_store cache
                    │  to_py)      │  Cache OWNS this reference
                    └──────┬───────┘
                           │  intptr_t (PyObject*, borrowed from cache)
                    ┌──────▼───────┐
                    │ py_callback  │  XINCREF args (refcnt: 1→2)
                    │  ::call()    │  Call Python function
                    │              │  XDECREF args (refcnt: 2→1, cache ref intact)
                    │              │  Return result (NEW reference, refcnt=1)
                    └──────┬───────┘
                           │  intptr_t (PyObject*, borrowed from cache)
                    ┌──────▼───────┐
                    │ output conv. │  Reads PyObject* value
                    │ (e.g. py_to_ │  Does NOT Py_DECREF
                    │  int)        │  Returns C++ value
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │   Observer   │  Uses C++ value
                    └──────────────┘
```

## Why Small Integers Mask the Bug

CPython caches small integers (-5 to 256) as immortal singletons. In
Python 3.12+, these have effectively infinite reference counts. An
incorrect `Py_DECREF` on a cached integer does not free it, so the
dangling pointer in the product store cache still points to a valid
object. This is why tests using only small integers (like `py:types`)
can pass even with incorrect reference counting.

Tests using floats (`py:coverage`), non-cached integers, or
`PhlexLifeline` objects (`py:vectypes`, `py:veclists`) expose the bug
because these objects have normal reference counts and are freed on
`Py_DECREF` to zero.

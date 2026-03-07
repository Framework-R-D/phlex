"""Tooling to simplify representation of C++ types in Python.

There is no canonical form of Python annotations and this gets even more
complicated with the existence of C++ types that have no representation in
Python. This tooling provides helpers to simplify what is presented to the
C++ side, and thus simplifies the maintenance of that code.
"""

import builtins
import ctypes
import typing
from types import UnionType
from typing import Any, Dict, Union

import numpy as np

__all__ = [
    "normalize_type",
]

# ctypes and numpy types are likely candidates for use in annotations
# TODO: should users be allowed to add to these?
_PY2CPP: dict[type, str] = {
    # numpy types
    np.bool_:             "bool",
    np.int8:              "int8_t",
    np.int16:             "int16_t",
    np.int32:             "int32_t",
    np.int64:             "int64_t",
    np.uint8:             "uint8_t",
    np.uint16:            "uint16_t",
    np.uint32:            "uint32_t",
    np.uint64:            "uint64_t",
    np.float32:           "float",
    np.float64:           "double",
    np.complex64:         "std::complex<float>",
    np.complex128:        "std::complex<double>",
    # the following types are aliased in numpy, so ignore here
    #np.intp:              "ptrdiff_t",
    #np.uintp:             "size_t",
}

# ctypes types that don't map cleanly to intN_t / uintN_t
_CTYPES_SPECIAL: dict[type, str] = {
    ctypes.c_bool:      "bool",
    ctypes.c_char:      "char",       # signedness is implementation-defined in C
    ctypes.c_wchar:     "wchar_t",
    ctypes.c_float:     "float",      # always IEEE 754 32-bit
    ctypes.c_double:    "double",     # always IEEE 754 64-bit
    ctypes.c_longdouble:"long double",# platform-dependent width, no stdint.h alias
    # the following types are aliased in ctypes, so ignore here
    #ctypes.c_size_t:    "size_t",
    #ctypes.c_ssize_t:   "ssize_t",
    ctypes.c_void_p:    "void*",
    ctypes.c_char_p:    "const char*",
    ctypes.c_wchar_p:   "const wchar_t*",
}

_CTYPES_INTEGER: list[type] = [
    ctypes.c_byte,  ctypes.c_ubyte,
    ctypes.c_short, ctypes.c_ushort,
    ctypes.c_int,   ctypes.c_uint,
    ctypes.c_long,  ctypes.c_ulong,
]

# (unsigned) long long may be aliased
if ctypes.c_longlong is not ctypes.c_long:
    _CTYPES_INTEGER.append(ctypes.c_longlong)
    _CTYPES_INTEGER.append(ctypes.c_ulonglong)

def _build_ctypes_map() -> dict[type, str]:
    result = dict(_CTYPES_SPECIAL)
    for tp in _CTYPES_INTEGER:
        bits = ctypes.sizeof(tp) * 8
        # _type_ uses struct format chars: lowercase = signed, uppercase = unsigned
        signed = tp._type_.islower()
        result[tp] = f"{'int' if signed else 'uint'}{bits}_t"
    return result

_PY2CPP.update(_build_ctypes_map())

# use ctypes to construct a mapping from platform types to exact types
_C2C: dict[str, str] = {
    "short":              _PY2CPP[ctypes.c_short],
    "unsigned short":     _PY2CPP[ctypes.c_ushort],
    "int":                _PY2CPP[ctypes.c_int],
    "unsigned int":       _PY2CPP[ctypes.c_uint],
    "long":               _PY2CPP[ctypes.c_long],
    "unsigned long":      _PY2CPP[ctypes.c_ulong],
    "long long":          'int64_t',
    "unsigned long long": 'uint64_t',
}


def normalize_type(tp: Any, globalns: Dict|None=None, localns: Dict|None=None) -> str:
    """Recursively normalize any Python annotation to a canonical name.

    This normalization supports:
      - raw types: int, str, float
      - typing generics: List[int], Dict[str, int], etc.
      - built-in generics (3.9+): list[int], dict[str, int]
      - string annotations: "list[int]", "list['int']"
      - NoneType

    Args:
        tp (Any): Some type annotation to normalize.
        globalns (dict): optional global namespace to resolve types.
        localns (dict): optional local namespace to resolve types.

    Returns:
        Canonical string representation of the type.
    """
    # most common case of some string; resolve it to handle `list[int]`, `list['int']`
    if isinstance(tp, str):
        ns = {**vars(builtins), **vars(typing)}
        if globalns:
            ns.update(globalns)
        if localns:
            ns.update(localns)
        try:
            tp = eval(tp, ns)
        except Exception:
            return _C2C.get(tp, tp)    # unresolvable

    # get the unsubscripted version of the type, or None if it's something unknown
    origin = typing.get_origin(tp)

    # sanity check: we have to reject union syntax, because the chosen C++ type must
    # be unambiguous (TODO: what with `Optional`?)
    if isinstance(tp, UnionType) or origin is Union:
        raise TypeError("To support C++, annotations passed to Phlex must be unambiguous")

    # TODO: debatable: maybe pointers should be considered arrays by default?
    if isinstance(tp, type) and issubclass(tp, ctypes._Pointer):
        raise TypeError("Pointers are ambiguous; declare an array if that is intended")

    # common case of forward references, from e.g. `List["double"]`
    if isinstance(tp, typing.ForwardRef):
        # ForwardRef.__forward_arg__ is the a string; recurse to resolve it
        return normalize_type(tp.__forward_arg__, globalns, localns)

    # special case for NoneType
    if tp is type(None):
        return "None"

    # clean up generic aliases, such as typing.List[int], list[int], etc.
    if origin is not None:
        args = typing.get_args(tp)

        if origin is np.ndarray:            # numpy arrays
            dtype_args = typing.get_args(args[1]) if len(args) >= 2 else ()
            return "ndarray[" + normalize_type(dtype_args[0], globalns, localns) + "]"

        if isinstance(origin, type):        # regular python typing type
            name = origin.__name__
        else:
            # fallback for unexpected origins
            name = getattr(origin, "__name__", repr(origin))

        return name + "[" + ",".join([normalize_type(a) for a in args]) + "]"

    # ctypes (fixed-size) array types
    try:
        if issubclass(tp, ctypes.Array):
            # TODO: tp._length_ may be useful as well
            return "array["+normalize_type(tp._type_, globalns, localns)+"]"
    except TypeError:
        pass            # tp is not a class

    # known builtin types representations from ctypes and numpy
    try:
        return _PY2CPP[tp]
    except KeyError:
        pass            # not a known Python type

    # fallback for plain Python types
    if isinstance(tp, type):
        return _C2C.get(tp.__name__, tp.__name__)

    # fallback for everything else, expecting repr() to be unique and consistent
    return repr(tp)


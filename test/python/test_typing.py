#!/usr/bin/env python3
"""Unit tests for phlex typing normalization."""

import ctypes
from typing import Dict, List

import numpy as np
import numpy.typing as npt
from phlex._typing import _C2C
from pytest import raises

from phlex import normalize_type


class TestTYPING:
    """Tests for phlex-specific typing."""

    def test_list_normalization(self):
        """Normalization of various forms of list annotations."""
        for types in (
            ["int", int, ctypes.c_int],
            ["unsigned int", ctypes.c_uint],
            ["long", ctypes.c_long],
            ["unsigned long", ctypes.c_ulong],
            ["long long", ctypes.c_longlong],
            ["unsigned long long", ctypes.c_ulonglong],
            ["float", float, ctypes.c_float],
            ["double", ctypes.c_double],
        ):
            # TODO: the use of _C2C here is a bit circular
            assert isinstance(types[0], str)
            tn = _C2C.get(types[0], types[0])

            if 0 < tn.find("_"):
                npt = tn[: tn.find("_")]
            elif tn == "float":
                npt = "float32"
            elif tn == "double":
                npt = "float64"
            npt = getattr(np, npt)

            norm = "list[" + tn + "]"
            for t in types + [npt]:
                assert normalize_type(norm) == norm
                try:
                    assert normalize_type(List[t]) == norm
                except SyntaxError as e:
                    # some of the above are not legal Python when used with List,
                    # but are fine when used with list; just ignore these as they
                    # will not show up in user code
                    if "Forward reference must be an expression" not in str(e):
                        raise
                assert normalize_type(list[t]) == norm

    def test_numpy_array_normalization(self):
        """Normalization of standard Numpy typing."""
        for t, s in (
            (np.bool_, "bool"),
            (np.int8, "int8_t"),
            (np.int16, "int16_t"),
            (np.int32, "int32_t"),
            (np.int64, "int64_t"),
            (np.uint8, "uint8_t"),
            (np.uint16, "uint16_t"),
            (np.uint32, "uint32_t"),
            (np.uint64, "uint64_t"),
            (np.float32, "float"),
            (np.float64, "double"),
        ):
            assert normalize_type(npt.NDArray[t]) == "ndarray[" + s + "]"

    def test_special_cases(self):
        """Special cases."""
        # None type
        assert normalize_type(None) == "None"

        # use of namespaces for evaluation
        assert ("foo") == "foo"
        global foo  # lgtm[py/unused-global-variable]
        foo = np.int64
        assert normalize_type("foo", globals()) == "int64_t"
        bar = np.int32
        assert normalize_type("bar", globals()) == "bar"
        assert normalize_type("bar", localns=locals()) == "int32_t"

        # union types are not allowed
        raises(TypeError, normalize_type, List[int] | Dict[int, int])

        # pointer types are not supported yet
        raises(TypeError, normalize_type, ctypes.POINTER(ctypes.c_int))

        # ctypes array
        assert normalize_type((ctypes.c_int * 42)) == "array[int32_t]"

        # something unknown
        class SomeType:
            def __repr__(self):
                return "some type"

        assert normalize_type(SomeType()) == "some type"

"""Algorithms exercising various numpy array types.

This test code implements algorithms that use numpy arrays of different types
to ensure that the Python bindings correctly handle them.
"""

import numpy as np
import numpy.typing as npt


def collectify_int32(i: int, j: int) -> npt.NDArray[np.int32]:
    """Create an int32 array from two integers."""
    return np.array([i, j], dtype=np.int32)


def sum_array_int32(coll: npt.NDArray[np.int32]) -> int:
    """Sum an int32 array."""
    if isinstance(coll, list):
        coll = np.array(coll, dtype=np.int32)
    return int(sum(int(x) for x in coll))


def collectify_uint32(
    i: "unsigned int", j: "unsigned int"  # type: ignore # noqa: F722
) -> npt.NDArray[np.uint32]:
    """Create a uint32 array from two integers."""
    return np.array([i, j], dtype=np.uint32)


def sum_array_uint32(coll: npt.NDArray[np.uint32]) -> "unsigned int":  # type: ignore # noqa: F722
    """Sum a uint32 array."""
    if isinstance(coll, list):
        coll = np.array(coll, dtype=np.uint32)
    return int(sum(int(x) for x in coll))


def collectify_int64(i: "long", j: "long") -> npt.NDArray[np.int64]:  # type: ignore # noqa: F821
    """Create an int64 array from two integers."""
    return np.array([i, j], dtype=np.int64)


def sum_array_int64(coll: npt.NDArray[np.int64]) -> "long":  # type: ignore # noqa: F821
    """Sum an int64 array."""
    if isinstance(coll, list):
        coll = np.array(coll, dtype=np.int64)
    return int(sum(int(x) for x in coll))


def collectify_uint64(
    i: "unsigned long", j: "unsigned long"  # type: ignore # noqa: F722
) -> npt.NDArray[np.uint64]:
    """Create a uint64 array from two integers."""
    return np.array([i, j], dtype=np.uint64)


def sum_array_uint64(coll: npt.NDArray[np.uint64]) -> "unsigned long":  # type: ignore # noqa: F722
    """Sum a uint64 array."""
    if isinstance(coll, list):
        coll = np.array(coll, dtype=np.uint64)
    return int(sum(int(x) for x in coll))


def collectify_float32(i: "float", j: "float") -> npt.NDArray[np.float32]:
    """Create a float32 array from two floats."""
    return np.array([i, j], dtype=np.float32)


def sum_array_float32(coll: npt.NDArray[np.float32]) -> "float":
    """Sum a float32 array."""
    return float(sum(coll))


def collectify_float64(i: "double", j: "double") -> npt.NDArray[np.float64]:  # type: ignore # noqa: F821
    """Create a float64 array from two floats."""
    return np.array([i, j], dtype=np.float64)


def sum_array_float64(coll: npt.NDArray[np.float64]) -> "double":  # type: ignore # noqa: F821
    """Sum a float64 array."""
    return float(sum(coll))


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    """Register algorithms for the test."""
    # int32
    m.transform(
        collectify_int32, input_family=config["input_int32"], output_products=["arr_int32"]
    )
    m.transform(
        sum_array_int32, input_family=["arr_int32"], output_products=config["output_int32"]
    )

    # uint32
    m.transform(
        collectify_uint32,
        input_family=config["input_uint32"],
        output_products=["arr_uint32"],
    )
    m.transform(
        sum_array_uint32,
        input_family=["arr_uint32"],
        output_products=config["output_uint32"],
    )

    # int64
    m.transform(
        collectify_int64, input_family=config["input_int64"], output_products=["arr_int64"]
    )
    m.transform(
        sum_array_int64, input_family=["arr_int64"], output_products=config["output_int64"]
    )

    # uint64
    m.transform(
        collectify_uint64,
        input_family=config["input_uint64"],
        output_products=["arr_uint64"],
    )
    m.transform(
        sum_array_uint64,
        input_family=["arr_uint64"],
        output_products=config["output_uint64"],
    )

    # float32
    m.transform(
        collectify_float32,
        input_family=config["input_float32"],
        output_products=["arr_float32"],
    )
    m.transform(
        sum_array_float32,
        input_family=["arr_float32"],
        output_products=config["output_float32"],
    )

    # float64
    m.transform(
        collectify_float64,
        input_family=config["input_float64"],
        output_products=["arr_float64"],
    )
    m.transform(
        sum_array_float64,
        input_family=["arr_float64"],
        output_products=config["output_float64"],
    )

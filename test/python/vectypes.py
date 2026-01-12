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
    i: "unsigned int",
    j: "unsigned int",  # type: ignore # noqa: F722
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
    i: "unsigned long",
    j: "unsigned long",  # type: ignore # noqa: F722
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


def collectify_float32_list(i: "float", j: "float") -> list[float]:
    """Create a float32 list from two floats."""
    return [i, j]


def collectify_float64_list(i: "double", j: "double") -> list["double"]:  # type: ignore # noqa: F821
    """Create a float64 list from two floats."""
    return [i, j]


def sum_array_float64(coll: npt.NDArray[np.float64]) -> "double":  # type: ignore # noqa: F821
    """Sum a float64 array."""
    return float(sum(coll))


def collectify_int32_list(i: int, j: int) -> list[int]:
    """Create an int32 list from two integers."""
    return [i, j]


def collectify_uint32_list(
    i: "unsigned int",
    j: "unsigned int",  # type: ignore # noqa: F722
) -> list[int]:
    """Create a uint32 list from two integers."""
    return [int(i), int(j)]


def collectify_int64_list(i: "long", j: "long") -> list[int]:  # type: ignore # noqa: F821
    """Create an int64 list from two integers."""
    return [int(i), int(j)]


def collectify_uint64_list(
    i: "unsigned long",
    j: "unsigned long",  # type: ignore # noqa: F722
) -> list[int]:
    """Create a uint64 list from two integers."""
    return [int(i), int(j)]


def sum_list_int32(coll: list[int]) -> int:
    """Sum a list of ints."""
    return sum(coll)


def sum_list_uint32(coll: list[int]) -> "unsigned int":  # type: ignore # noqa: F722
    """Sum a list of uints."""
    return sum(coll)


def sum_list_int64(coll: list[int]) -> "long":  # type: ignore # noqa: F821
    """Sum a list of longs."""
    return sum(coll)


def sum_list_uint64(coll: list[int]) -> "unsigned long":  # type: ignore # noqa: F722
    """Sum a list of ulongs."""
    return sum(coll)


def sum_list_float(coll: list[float]) -> float:
    """Sum a list of floats."""
    return sum(coll)


def sum_list_double(coll: list["double"]) -> "double":  # type: ignore # noqa: F821
    """Sum a list of doubles."""
    return float(sum(coll))


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    """Register algorithms for the test."""
    try:
        use_lists = config["use_lists"]
    except (KeyError, TypeError):
        use_lists = False

    # int32
    m.transform(
        collectify_int32_list if use_lists else collectify_int32,
        input_family=config["input_int32"],
        output_products=["arr_int32"],
    )
    m.transform(
        sum_list_int32 if use_lists else sum_array_int32,
        input_family=["arr_int32"],
        output_products=config["output_int32"],
        name="sum_int32",
    )

    # uint32
    m.transform(
        collectify_uint32_list if use_lists else collectify_uint32,
        input_family=config["input_uint32"],
        output_products=["arr_uint32"],
    )
    m.transform(
        sum_list_uint32 if use_lists else sum_array_uint32,
        input_family=["arr_uint32"],
        output_products=config["output_uint32"],
        name="sum_uint32",
    )

    # int64
    m.transform(
        collectify_int64_list if use_lists else collectify_int64,
        input_family=config["input_int64"],
        output_products=["arr_int64"],
    )
    m.transform(
        sum_list_int64 if use_lists else sum_array_int64,
        input_family=["arr_int64"],
        output_products=config["output_int64"],
        name="sum_int64",
    )

    # uint64
    m.transform(
        collectify_uint64_list if use_lists else collectify_uint64,
        input_family=config["input_uint64"],
        output_products=["arr_uint64"],
    )
    m.transform(
        sum_list_uint64 if use_lists else sum_array_uint64,
        input_family=["arr_uint64"],
        output_products=config["output_uint64"],
        name="sum_uint64",
    )

    # float32
    m.transform(
        collectify_float32_list if use_lists else collectify_float32,
        input_family=config["input_float32"],
        output_products=["arr_float32"],
    )
    m.transform(
        sum_list_float if use_lists else sum_array_float32,
        input_family=["arr_float32"],
        output_products=config["output_float32"],
    )

    # float64
    m.transform(
        collectify_float64_list if use_lists else collectify_float64,
        input_family=config["input_float64"],
        output_products=["arr_float64"],
    )
    m.transform(
        sum_list_double if use_lists else sum_array_float64,
        input_family=["arr_float64"],
        output_products=config["output_float64"],
    )

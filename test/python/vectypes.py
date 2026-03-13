"""Algorithms exercising various numpy array types.

This test code implements algorithms that use numpy arrays of different types
to ensure that the Python bindings correctly handle them.
"""

import numpy as np
import numpy.typing as npt

# Type aliases for C++ types that don't have Python equivalents
# These are used by the C++ wrapper to identify the correct converter


def collectify_int32(i: int, j: int) -> npt.NDArray[np.int32]:
    """Create an int32 array from two integers."""
    return np.array([i, j], dtype=np.int32)


def sum_array_int32(coll: npt.NDArray[np.int32]) -> int:
    """Sum an int32 array."""
    return int(sum(int(x) for x in coll))


def collectify_uint32(
    u1: "unsigned int",  # type: ignore # noqa: F722 # noqa: F821
    u2: "unsigned int",  # type: ignore # noqa: F722 # noqa: F821
) -> npt.NDArray[np.uint32]:
    """Create a uint32 array from two integers."""
    return np.array([u1, u2], dtype=np.uint32)


def sum_array_uint32(coll: npt.NDArray[np.uint32]) -> np.uint32:
    """Sum a uint32 array."""
    return np.uint32(sum(int(x) for x in coll))


def collectify_int64(l1: "long", l2: "long") -> npt.NDArray[np.int64]:  # type: ignore # noqa: F821
    """Create an int64 array from two integers."""
    return np.array([l1, l2], dtype=np.int64)


def sum_array_int64(coll: npt.NDArray[np.int64]) -> "int64_t":  # type: ignore # noqa: F821
    """Sum an int64 array."""
    return np.int64(sum(int(x) for x in coll))


def collectify_uint64(
    ul1: "unsigned long",  # type: ignore # noqa: F722 # noqa: F821
    ul2: "unsigned long",  # type: ignore # noqa: F722 # noqa: F821
) -> npt.NDArray[np.uint64]:
    """Create a uint64 array from two integers."""
    return np.array([ul1, ul2], dtype=np.uint64)


def sum_array_uint64(coll: npt.NDArray[np.uint64]) -> "uint64_t":  # type: ignore # noqa: F821
    """Sum a uint64 array."""
    return np.uint64(sum(int(x) for x in coll))


def collectify_float32(f1: float, f2: float) -> npt.NDArray[np.float32]:
    """Create a float32 array from two floats."""
    return np.array([f1, f2], dtype=np.float32)


def sum_array_float32(coll: npt.NDArray[np.float32]) -> np.float32:
    """Sum a float32 array."""
    return np.float32(sum(coll))


def collectify_float64(d1: "double", d2: "double") -> npt.NDArray[np.float64]:  # type: ignore # noqa: F821
    """Create a float64 array from two floats."""
    return np.array([d1, d2], dtype=np.float64)


def collectify_float32_list(f1: float, f2: float) -> list[float]:
    """Create a float32 list from two floats."""
    return [f1, f2]


def collectify_float64_list(d1: "double", d2: "double") -> list["double"]:  # type: ignore # noqa: F821
    """Create a float64 list from two floats."""
    return [d1, d2]


def sum_array_float64(coll: npt.NDArray[np.float64]) -> "double":  # type: ignore # noqa: F821
    """Sum a float64 array."""
    return float(sum(coll))


def collectify_int32_list(i: int, j: int) -> list[int]:
    """Create an int32 list from two integers."""
    return [i, j]


def collectify_uint32_list(
    u1: "unsigned int",  # type: ignore # noqa: F722 # noqa: F821
    u2: "unsigned int",  # type: ignore # noqa: F722 # noqa: F821
) -> list["unsigned int"]:  # type: ignore # noqa: F722 # noqa: F821
    """Create a uint32 list from two integers."""
    return [np.uint32(u1), np.uint32(u2)]


def collectify_int64_list(l1: "long", l2: "long") -> "list[int64_t]":  # type: ignore # noqa: F821
    """Create an int64 list from two integers."""
    return [l1, l2]


def collectify_uint64_list(
    ul1: "unsigned long",  # type: ignore # noqa: F722 # noqa: F821
    ul2: "unsigned long",  # type: ignore # noqa: F722 # noqa: F821
) -> "list[uint64_t]":  # type: ignore # noqa: F821
    """Create a uint64 list from two integers."""
    return [np.uint64(ul1), np.uint64(ul2)]


def sum_list_int32(coll: list[int]) -> int:
    """Sum a list of ints."""
    return sum(coll)


def sum_list_uint32(coll: "list[uint32_t]") -> "uint32_t":  # type: ignore # noqa: F821
    """Sum a list of uints."""
    return sum(coll)


def sum_list_int64(coll: "list[int64_t]") -> "int64_t":  # type: ignore # noqa: F821
    """Sum a list of longs."""
    return sum(coll)


def sum_list_uint64(coll: "list[uint64_t]") -> "uint64_t":  # type: ignore # noqa: F821
    """Sum a list of ulongs."""
    return sum(coll)


def sum_list_float(coll: list[float]) -> float:
    """Sum a list of floats."""
    return np.float32(sum(coll))


def sum_list_double(coll: list["double"]) -> "double":  # type: ignore # noqa: F821
    """Sum a list of doubles."""
    return sum(coll)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms for the test."""
    try:
        use_lists = config["use_lists"]
    except (KeyError, TypeError):
        use_lists = False

    specs = [
        (
            "int32",
            collectify_int32_list,
            collectify_int32,
            sum_list_int32,
            sum_array_int32,
            "input_int32",
            "output_int32",
            "sum_int32",
        ),
        (
            "uint32",
            collectify_uint32_list,
            collectify_uint32,
            sum_list_uint32,
            sum_array_uint32,
            "input_uint32",
            "output_uint32",
            "sum_uint32",
        ),
        (
            "int64",
            collectify_int64_list,
            collectify_int64,
            sum_list_int64,
            sum_array_int64,
            "input_int64",
            "output_int64",
            "sum_int64",
        ),
        (
            "uint64",
            collectify_uint64_list,
            collectify_uint64,
            sum_list_uint64,
            sum_array_uint64,
            "input_uint64",
            "output_uint64",
            "sum_uint64",
        ),
        (
            "float32",
            collectify_float32_list,
            collectify_float32,
            sum_list_float,
            sum_array_float32,
            "input_float32",
            "output_float32",
            None,
        ),
        (
            "float64",
            collectify_float64_list,
            collectify_float64,
            sum_list_double,
            sum_array_float64,
            "input_float64",
            "output_float64",
            None,
        ),
    ]

    for name, list_collect, arr_collect, list_sum, arr_sum, in_key, out_key, sum_name in specs:
        arr_name = f"arr_{name}"
        m.transform(
            list_collect if use_lists else arr_collect,
            input_family=config[in_key],
            output_products=[arr_name],
        )

        sum_kwargs = {
            "input_family": [
                {
                    "creator": list_collect.__name__ if use_lists else arr_collect.__name__,
                    "layer": "event",
                    "suffix": arr_name,
                }
            ],
            "output_products": config[out_key],
        }
        if sum_name:
            sum_kwargs["name"] = sum_name

        m.transform(list_sum if use_lists else arr_sum, **sum_kwargs)

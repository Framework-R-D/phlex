"""Algorithms exercising various C++ types.

This test code implements algorithms that use types other than the standard
int/string to ensure that the Python bindings correctly handle them.
"""

import numpy as np
import numpy.typing as npt


class double(float):  # noqa: N801
    """Dummy class for C++ double type."""

    pass


def add_float(i: float, j: float) -> float:
    """Add two floats.

    Args:
        i (float): First input.
        j (float): Second input.

    Returns:
        float: Sum of the two inputs.
    """
    return i + j


def add_double(i: double, j: double) -> double:
    """Add two doubles.

    Args:
        i (float): First input.
        j (float): Second input.

    Returns:
        float: Sum of the two inputs.
    """
    return double(i + j)


def add_unsigned(i: "unsigned int", j: "unsigned int") -> "unsigned int":  # type: ignore  # noqa: F722
    """Add two unsigned integers.

    Args:
        i (int): First input.
        j (int): Second input.

    Returns:
        int: Sum of the two inputs.
    """
    return i + j


def collect_float(i: float, j: float) -> npt.NDArray[np.float32]:
    """Combine floats into a numpy array.

    Args:
        i (float): First input.
        j (float): Second input.

    Returns:
        ndarray: Array of floats.
    """
    return np.array([i, j], dtype=np.float32)


def collect_double(i: double, j: double) -> npt.NDArray[np.float64]:
    """Combine doubles into a numpy array.

    Args:
        i (float): First input.
        j (float): Second input.

    Returns:
        ndarray: Array of doubles.
    """
    return np.array([i, j], dtype=np.float64)


def and_bool(i: bool, j: bool) -> bool:
    """And two booleans.

    Args:
        i (bool): First input.
        j (bool): Second input.

    Returns:
        bool: Logical AND of the two inputs.
    """
    return i and j


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(
        add_float, input_family=config["input_float"], output_products=config["output_float"]
    )

    m.transform(
        add_double, input_family=config["input_double"], output_products=config["output_double"]
    )

    m.transform(
        add_unsigned, input_family=config["input_uint"], output_products=config["output_uint"]
    )

    m.transform(and_bool, input_family=config["input_bool"], output_products=config["output_bool"])

    m.transform(
        collect_float, input_family=config["input_float"], output_products=config["output_vfloat"]
    )

    m.transform(
        collect_double,
        input_family=config["input_double"],
        output_products=config["output_vdouble"],
    )

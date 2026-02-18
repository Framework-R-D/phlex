"""Algorithms exercising various C++ types.

This test code implements algorithms that use types other than the standard
int/string to ensure that the Python bindings correctly handle them.
"""

import numpy as np
import numpy.typing as npt


class double(float):  # noqa: N801
    """Dummy class for C++ double type."""

    pass


def add_float(f1: float, f2: float) -> float:
    """Add two floats.

    Args:
        f1 (float): First input.
        f2 (float): Second input.

    Returns:
        float: Sum of the two inputs.
    """
    return f1 + f2


def add_double(d1: double, d2: double) -> double:
    """Add two doubles.

    Args:
        d1 (double): First input.
        d2 (double): Second input.

    Returns:
        double: Sum of the two inputs.
    """
    return double(d1 + d2)


def add_unsigned(u1: "unsigned int", u2: "unsigned int") -> "unsigned int":  # type: ignore  # noqa: F722
    """Add two unsigned integers.

    Args:
        u1 (unsigned int): First input.
        u2 (unsigned int): Second input.

    Returns:
        unsigned int: Sum of the two inputs.
    """
    return u1 + u2


def collect_float(f1: float, f2: float) -> npt.NDArray[np.float32]:
    """Combine floats into a numpy array.

    Args:
        f1 (float): First input.
        f2 (float): Second input.

    Returns:
        ndarray: Array of floats.
    """
    return np.array([f1, f2], dtype=np.float32)


def collect_double(d1: double, d2: double) -> npt.NDArray[np.float64]:
    """Combine doubles into a numpy array.

    Args:
        d1 (double): First input.
        d2 (double): Second input.

    Returns:
        ndarray: Array of doubles.
    """
    return np.array([d1, d2], dtype=np.float64)


def and_bool(b1: bool, b2: bool) -> bool:
    """And two booleans.

    Args:
        b1 (bool): First input.
        b2 (bool): Second input.

    Returns:
        bool: Logical AND of the two inputs.
    """
    return b1 and b2


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

"""A most basic algorithm using a type that is not a C++ builtin.

This test code implements the smallest possible run that does something
real with a non-builtin type: it exercises numpy support by using it as
output and input of algorithms.
"""

import numpy as np
import numpy.typing as npt


def collectify(i: int, j: int) -> npt.NDArray[np.int32]:
    """Combine the inputs into an array.

    Create a numpy array of 2 elements from the separate inputs.

    Args:
        i (int): First input.
        j (int): Second input.

    Returns:
        ndarray: Array represntation of the two inputs.

    Examples:
        >>> collectify(1, 2)
        array([1, 2], dtype=int32)
    """
    return np.array([i, j], dtype=np.int32)


def sum_array(coll: npt.NDArray[np.int32]) -> int:
    """Add the elements of the input collection and return the sum total.

    Use the builtin `sum` function to add the elements from the input
    collection to arrive at their total.

    Args:
        coll (ndarray): numpy array of input values.

    Returns:
        int: Sum of the elements of the input collection.

    Examples:
        >>> sum_array(np.array[1, 2]))
        3
    """
    return sum(coll)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms exercising numpy arrays.

    Use the standard Phlex `transform` registration to insert a node
    in the execution graph that creates and one that receives a numpy
    array. The final result is the sum of the elements as an ouput. The
    labels of inputs and outputs are taken from the configuration.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(collectify, input_family=config["input"], output_products=["my_pyarray"])
    m.transform(sum_array, input_family=["my_pyarray"], output_products=config["output"])

import numpy as np
import numpy.typing as npt


def sum_coll(coll: npt.NDArray[np.int32]) -> int:
    """Add the elements of the input collection and return the sum total.

    Use the builtin `sum` function to add the elements from the input
    collection to arrive at their total.

    Args:
        coll (ndarray): numpy array of input values.

    Returns:
        int: Sum of the elements of the input collection.

    Examples:
        >>> sum_coll(np.array[1, 2]))
        3
    """

    return sum(coll)


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    """Register the `sum_coll` algorithm as a transformation.

    Use the standard Phlex `transform` registration to insert a node
    in the execution graph that receives an input collection and
    produces the sum of its elements as an ouput. The labels of inputs
    and outputs are taken from the configuration.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(sum_coll,
                input_family = config["input"],
                output_products = config["output"])


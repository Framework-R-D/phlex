"""A most basic algorithm.

This test code implements the smallest possible run that does something
real. It serves as a "Hello, World" equivalent for running Python code.
"""


def add(i: int, j: int) -> int:
    """Add the inputs together and return the sum total.

    Use the standard `+` operator to add the two inputs together
    to arrive at their total.

    Args:
        i (int): First input.
        j (int): Second input.

    Returns:
        int: Sum of the two inputs.

    Examples:
        >>> add(1, 2)
        3
    """
    return i + j


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register the `add` algorithm as a transformation.

    Use the standard Phlex `transform` registration to insert a node
    in the execution graph that receives two inputs and produces their
    sum as an ouput. The labels of inputs and outputs are taken from
    the configuration.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(add, input_family=config["input"], output_products=config["output"])

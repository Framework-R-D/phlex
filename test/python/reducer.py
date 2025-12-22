"""Explicit reduction algorithm.

This test code implements a reduction by explicitly coding it up in the
Phlex execution graph. The goal is to try to creating node naming issues
to ensure these either don't happen or are properly resolved by Phlex:

 - Re-uses the same Python algorithm in different nodes, this means that
   simply taking the name from reflection can not be relied upon.
 - Several Python algorithms receive the same C++ inputs, which causes
   either converter node naming problems (if converters are reused) or
   lifetime issues (if they are not).
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
    """Register a series of `add` algorithm as transformations.

    Use the standard Phlex `transform` registration to insert nodes
    in the execution graph that receive two inputs and produces their
    sum as an ouput. The idea is a workflow from multiple independent
    sources and make sure the GIL is properly handed off such that no
    live-locks occur.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    # first recieve the same input x4 but return "different" output
    for i in range(4):
        m.transform(
            add, name="reduce%d" % i, input_family=config["input"], output_products=["sum%d" % i]
        )

    # now reduce them pair-wise
    m.transform(add, name="reduce01", input_family=["sum0", "sum1"], output_products=["sum01"])
    m.transform(add, name="reduce23", input_family=["sum2", "sum3"], output_products=["sum23"])

    # once more (and the configuration will add a verifier)
    m.transform(add, name="reduce", input_family=["sum01", "sum23"], output_products=["sum"])

"""A most basic algorithm.

This test code implements the smallest possible run that does something
real. It serves as a "Hello, World" equivalent for running Python code.
"""

from typing import Protocol, TypeVar

from variant import Variant


class AddableProtocol[T](Protocol):
    """Typer bound for any types that can be added."""

    def __add__(self, other: T) -> T:  # noqa: D105
        ...  # codeql[py/ineffectual-statement]


Addable = TypeVar("Addable", bound=AddableProtocol)


def add(i: Addable, j: Addable) -> Addable:
    """Add the inputs together and return the sum total.

    Use the standard `+` operator to add the two inputs together
    to arrive at their total.

    Args:
        i (Addable): First input.
        j (Addable): Second input.

    Returns:
        Addable: Sum of the two inputs.

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
    int_adder = Variant(add, {"i": int, "j": int, "return": int}, "iadd")
    m.transform(int_adder, input_family=config["input"], output_products=config["output"])

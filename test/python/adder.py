"""A most basic algorithm.

This test code implements the smallest possible run that does something
real. It serves as a "Hello, World" equivalent for running Python code.
"""

from typing import Any, Protocol, TypeVar, cast

from phlex import Variant


class AddableProtocol[T](Protocol):
    """Typer bound for any types that can be added."""

    def __add__(self, other: T) -> T:  # noqa: D105
        ...  # codeql[py/ineffectual-statement]


Addable = TypeVar("Addable", bound=AddableProtocol)

_DEFAULT_ADDABLE = cast(Any, 42)


def add(i: Addable, j: Addable = _DEFAULT_ADDABLE) -> Addable:
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
    int_adder = Variant(add, {"i": int, "j": int, "return": int}, config.get("name", "iadd"))

    # run a few cases that are supposed to fail cleanly
    for kwargs, err, msg in [
        (
            {"input_family": config["input"]},
            "should have an output suffix",
            "without an output suffix",
        ),
        (
            {"input_family": config["input"], "output_product_suffixes": [42]},
            "must be a string",
            "with non-string output suffix",
        ),
        (
            {"input_family": object(), "output_product_suffixes": ["42"]},
            "input_family must be a sequence",
            "with non-sequence input family",
        ),
        (
            {"input_family": config["input"], "output_product_suffixes": object()},
            "output_product_suffixes must be a sequence",
            "with non-sequence output suffixes",
        ),
        (
            {"input_family": [{"suffix": 42}], "output_product_suffixes": ["42"]},
            "missing suffix or not a string",
            "with non-string input suffix",
        ),
        # currently an error, but should be supported at some point:
        (
            {"input_family": config["input"], "output_product_suffixes": ["a", "b"]},
            "only a single output supported",
            "with multiple output suffixes",
        ),
    ]:
        try:
            m.transform(int_adder, **kwargs)
        except TypeError as e:
            assert err in str(e)
        else:
            raise AssertionError("m.transform() should reject registrations " + msg)

    # functional transform registration
    m.transform(int_adder, input_family=config["input"], output_product_suffixes=config["output"])

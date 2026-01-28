"""Test mismatch between input labels and types."""


def mismatch_func(a: int, b: int):
    """Add two integers."""
    return a + b


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms."""
    # input_family has 1 element, but function takes 2 arguments
    # This should trigger the error in modulewrap.cpp
    m.transform(mismatch_func, input_family=["a"], output_products=["sum"])

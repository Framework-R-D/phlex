"""Test coverage for list input converters."""


class double(float):  # noqa: N801
    """Dummy class for C++ double type."""

    pass


def list_int_func(lst: list[int]) -> int:
    """Sum a list of integers."""
    return sum(lst)


def list_float_func(lst: list[float]) -> float:
    """Sum a list of floats."""
    return sum(lst)


# For double, I'll use string annotation to be safe and match C++ check
def list_double_func(lst: "list[double]") -> float:  # type: ignore
    """Sum a list of doubles."""
    return sum(lst)


def collect_int(i: int) -> list[int]:
    """Collect an integer into a list."""
    return [i]


def collect_float(f: float) -> list[float]:
    """Collect a float into a list."""
    return [f]


def collect_double(d: "double") -> "list[double]":  # type: ignore
    """Collect a double into a list."""
    return [d]


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms."""
    # We need to transform scalar inputs to lists first
    # i, f1, d1 come from cppsource4py
    m.transform(collect_int, input_family=["i"], output_products=["l_int"])
    m.transform(collect_float, input_family=["f1"], output_products=["l_float"])
    m.transform(collect_double, input_family=["d1"], output_products=["l_double"])

    m.transform(list_int_func, input_family=["l_int"], output_products=["sum_int"])
    m.transform(list_float_func, input_family=["l_float"], output_products=["sum_float"])
    m.transform(list_double_func, input_family=["l_double"], output_products=["sum_double"])

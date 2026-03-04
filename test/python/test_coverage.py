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


def collect_float(f1: float) -> list[float]:
    """Collect a float into a list."""
    return [f1]


def collect_double(d1: "double") -> "list[double]":  # type: ignore
    """Collect a double into a list."""
    return [d1]


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms."""
    # We need to transform scalar inputs to lists first
    # i, f1, d1 come from cppsource4py
    tfs = ((collect_int, "input", "i", "l_int"),
           (collect_float, "input", "f1", "l_float"),
           (collect_double, "input", "d1", "l_double"),
           (list_int_func, collect_int.__name__, "l_int", "sum_int"),
           (list_float_func, collect_float.__name__, "l_float", "sum_float"),
           (list_double_func, collect_double.__name__, "l_double", "sum_double")
          )

    for func, creator, suffix, output in tfs:
        input_family = [{"creator": creator, "layer": "event", "suffix": suffix}]
        m.transform(func, input_family=input_family, output_products=[output])

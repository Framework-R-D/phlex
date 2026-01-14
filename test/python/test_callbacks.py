"""Test coverage gaps in modulewrap.cpp."""


# 3-argument function to trigger py_callback<3>
def sum_three(a: int, b: int, c: int) -> int:
    """Sum three integers."""
    return a + b + c


# Function that raises exception to test error handling
def raise_error(a: int) -> int:
    """Raise a RuntimeError."""
    raise RuntimeError("Intentional failure")


# Invalid bool return (2)
def bad_bool(a: int) -> bool:
    """Return an invalid boolean value."""
    return 2  # type: ignore


# Invalid long return (float)
def bad_long(a: int) -> "long":  # type: ignore # noqa: F821
    """Return a float instead of an int."""
    return 1.5  # type: ignore



# Invalid uint return (negative)
def bad_uint(a: int) -> "unsigned int":  # type: ignore # noqa: F722
    """Return a negative value for unsigned int."""
    return -5  # type: ignore


# Function with mismatching annotation count vs config inputs
def two_args(a: int, b: int) -> int:
    """Sum two integers."""
    return a + b


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms based on configuration."""
    try:
        mode = config["mode"]
    except KeyError:
        mode = "three_args"

    if mode == "three_args":
        m.transform(sum_three, input_family=config["input"], output_products=config["output"])
    elif mode == "exception":
        m.transform(raise_error, input_family=config["input"], output_products=config["output"])
    elif mode == "bad_bool":
        m.transform(bad_bool, input_family=config["input"], output_products=config["output"])
    elif mode == "bad_long":
        m.transform(bad_long, input_family=config["input"], output_products=config["output"])
    elif mode == "bad_uint":
        m.transform(bad_uint, input_family=config["input"], output_products=config["output"])
    elif mode == "mismatch":
        m.transform(two_args, input_family=config["input"], output_products=config["output"])

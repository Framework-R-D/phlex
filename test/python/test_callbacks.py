"""Test coverage gaps in modulewrap.cpp."""

import numpy as np
import numpy.typing as npt

# 3-argument function to trigger py_callback<3>
def sum_three(a: int, b: int, c: int) -> int:
    return a + b + c

# Function that raises exception to test error handling
def raise_error(a: int) -> int:
    raise RuntimeError("Intentional failure")

# Invalid bool return (2)
def bad_bool(a: int) -> bool:
    return 2 # type: ignore

# Invalid long return (float)
def bad_long(a: int) -> int:
    return 1.5 # type: ignore

class unsigned_int(int):
    pass

# Invalid uint return (negative)
def bad_uint(a: int) -> unsigned_int:
    return -5 # type: ignore

def PHLEX_REGISTER_ALGORITHMS(m, config):
    try:
        mode = config["mode"]
    except KeyError:
        mode = "three_args"

    if mode == "three_args":
        m.transform(
            sum_three,
            input_family=config["input"],
            output_products=config["output"]
        )
    elif mode == "exception":
        m.transform(
            raise_error,
            input_family=config["input"],
            output_products=config["output"]
        )
    elif mode == "bad_bool":
        m.transform(
            bad_bool,
            input_family=config["input"],
            output_products=config["output"]
        )
    elif mode == "bad_long":
        m.transform(
            bad_long,
            input_family=config["input"],
            output_products=config["output"]
        )
    elif mode == "bad_uint":
        m.transform(
            bad_uint,
            input_family=config["input"],
            output_products=config["output"]
        )

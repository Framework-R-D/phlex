"""Basic Numba tests for all supported types.

Smallest possible tests with a mixture of Python and Numba: Python
providers to produce data, Numba algorithms to transform them, and Python
observers for verification.
"""

import numba
import numpy as np
from adder import add

# arg0 suff, arg1 suff, type, result
specs = (
    ("i", "j", np.int32, 1),
    ("u1", "u2", np.uint32, 1),
    ("l1", "l2", np.int64, 1),
    ("ul1", "ul2", np.uint64, 100),
    ("f1", "f2", np.float32, 1.0),
    ("d1", "d2", np.float64, 1.0),
)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register Numba-jited `add` algorithm variants as a transformation.

    Use the standard Phlex `transform` registration to insert a node in the
    execution graph of a Numba-jited Python function that receives two inputs
    and produces their sum as an output.

    Similarly, use the standard Phlex `observe` to add verifier nodes.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """

    def new_o(x):
        def o(y):
            assert y == x

        return o

    for arg0, arg1, t, res in specs:
        tn = t.__name__

        # GIL-free transforms, which run 100% in C/C++
        f_a = numba.cfunc(f"{tn}({tn}, {tn})", nogil=True, nopython=True, cache=True)(add)
        m.transform(
            f_a,
            name="add_" + tn,
            input_family=[
                {"creator": "input", "layer": "event", "suffix": arg0},
                {"creator": "input", "layer": "event", "suffix": arg1},
            ],
            output_product_suffixes=["sum_" + tn],
            concurrency=4,
        )

        # GIL-free observers
        f_o = numba.cfunc(f"void({tn})", nogil=True, nopython=True, cache=True)(new_o(res))
        m.observe(
            f_o,
            name="obs_" + tn,
            input_family=[{"creator": "add_" + tn, "layer": "event", "suffix": "sum_" + tn}],
            concurrency=4,
        )

    # GIL-releasing transform, called through the wrapper; this is a special case
    # to hit the thread state caching code and only tested on ints, since all types
    # follow the same path
    f_aw = numba.njit("int32(int32, int32)", nogil=True, cache=True)(add)
    def wrapped_gilreleasing(i: int, j: int) -> int:
        return f_aw(i, j)

    m.transform(
        wrapped_gilreleasing,
        name="wrapped_add_int32",
        input_family=[
            {"creator": "input", "layer": "event", "suffix": "i"},
            {"creator": "input", "layer": "event", "suffix": "j"},
        ],
        output_product_suffixes=["wrapped_sum_int32"],
        concurrency=4,
    )


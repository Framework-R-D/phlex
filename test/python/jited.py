"""A most basic algorithm compiled with Numba.

This test code implements the smallest possible run that does something
real. It serves as a "Hello, World" equivalent for running Python code
that is compiled with Numba.
"""

import numba.core.decorators as nb_dec

from adder import add


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
    nb_add = nb_dec.cfunc("int32(int32, int32)", nogil=True, nopython=True, cache=True)(add)
    m.transform(nb_add, input_family=config["input"], output_product_suffixes=config["output"],
                concurrency = 128)


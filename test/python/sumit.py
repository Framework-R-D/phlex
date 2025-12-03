from phlex.registry import register

import numpy as np
import numpy.typing as npt


def sum_coll(coll: npt.NDArray[np.int32]) -> int:
    return sum(coll)


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    register(sum_coll, m, config)


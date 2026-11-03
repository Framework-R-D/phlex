"""Algorithms exercising various C++ types.

Test UNsupported types/type combinations.

Python has dynamic typing, meaning that type contracts are not binding and need
to be (re-)verified at runtime. If types are incompatible, there should be a
proper error message and a graceful shutdown. Since these errors will happen
_after_ Phlex graph execution starts, we'll need to run a single phlex job for
each and check for the error message in the log file.

To add a new faulty check, add a row to FAULTY_CASES. CMake picks up the new
faulty case automatically.
"""

# the code below has a "table-like" code blocks that simply looks way better
# ignoring the line-length cut-off, so disable this one check for the whole file
# ruff: noqa: E501

import math
from dataclasses import dataclass
from typing import Any

import numpy as np
import numpy.typing as npt
from basic_types import TYPES

from phlex import Variant


# faulty cases that should be detected
@dataclass(frozen=True)
class FaultyCase:
    """A malformed value returned under a well-formed annotation.

    Attributes:
        tag: Unique name; also the CTest name suffix.
        annotation: The (valid) return annotation the transform claims.
        value: The (invalid) value the transform actually returns.
    """

    tag: str
    annotation: Any
    value: Any


# fmt: off
FAULTY_CASES = [
    #           tag                     claimed annotation          actual value
    FaultyCase("mixed_list_float_str",  list[float],                [1.0, "two"]),
    FaultyCase("mixed_list_uint_float", list["unsigned int"],       [1, 2.5]), # type: ignore # noqa: F722
    FaultyCase("nested_list",           list["double"],             [[1.0], [2.0]]), # type: ignore # noqa: F821
    FaultyCase("unicode_as_float",      float,                      "π"),
    FaultyCase("unicode_digit_as_uint", "unsigned int",             "٣"),  # Arabic-Indic 3
    FaultyCase("unicode_in_list",       list["double"],             ["1.0", "∞"]), # type: ignore # noqa: F821
    FaultyCase("emoji_as_bool",         bool,                       "✅"),
    FaultyCase("negative_uint",         "unsigned int",             -1),
    FaultyCase("overflow_uint",         "unsigned int",             2**65),
    FaultyCase("none_as_double",        "double",                   None),
    FaultyCase("numpy_str_as_float32",  npt.NDArray[np.float32],    np.array(["a", "b"])),
    FaultyCase("numpy_object_mixed",    npt.NDArray[np.float64],    np.array([1.0, "x"], dtype=object)),
]
# fmt: on


def _faulty_product(case: FaultyCase) -> str:
    return f"faulty_{case.tag}"

def _check_unique_names():
    names = [_faulty_product(g) for g in FAULTY_CASES]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise ValueError(f"duplicate generated product names: {sorted(dupes)}")

_check_unique_names()
_FAULTY_BY_TAG = {g.tag: g for g in FAULTY_CASES}

def _same_values(got, want, rel_tol: float) -> bool:
    """Compare scalars, numpy arrays, and lists element-wise."""
    got = np.atleast_1d(np.asarray(got)).tolist()
    want = np.atleast_1d(np.asarray(want)).tolist()
    return len(got) == len(want) and all(
        math.isclose(g, w, rel_tol=rel_tol, abs_tol=0.0)
        for g, w in zip(got, want, strict=True)
    )

def _input(config, suffix: str) -> dict:
    return {"creator": config["creator"], "layer": config["layer"], "suffix": suffix}


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms.

    The config key ``case`` selects either the full valid suite (``"valid"``)
    or a single faulty case by tag.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    tag = config["tag"]
    case = _FAULTY_BY_TAG[tag]
    product = _faulty_product(case)
    trigger = TYPES[0]  # any real input works; it only drives the schedule

    def produce(x):
        return case.value

    def sink(x):
        pass

    m.transform(
        Variant(produce, {"x": trigger.annotation, "return": case.annotation}, product),
        input_family=[_input(config, trigger.inputs[0])],
        output_product_suffixes=[product],
    )
    m.observe(
        Variant(sink, {"x": case.annotation}, f"sink_{product}"),
        input_family=[
            {"creator": product, "layer": config["layer"], "suffix": product},
        ],
    )


"""Algorithms exercising various C++ types.

Test supported basic C++ types. Algorithms are generated from the following:

    TYPES          supported C++ types
    OUTPUT_KINDS   supported output types (scalar, numpy, list)

Note: array inputs are tested separately (see vectypes.py).

Each (type, output kind) pair produces a transform named ``make_<kind>_<type>``,
which writes the product ``<kind>_<type>``. It also produces an observer named
``verify_<kind>_<type>``, which verifies the expected result and raises if differs.

To support a new type, add a row to TYPES as well as to source.cpp (the label names
should match for the inputs).
"""

# the code below has several "table-like" code blocks that simply look way better
# ignoring the line-length cut-off, so disable this one check for the whole file
# ruff: noqa: E501

import math
from dataclasses import dataclass
from typing import Any, Callable

import numpy as np
import numpy.typing as npt


# supported types table
@dataclass(frozen=True)
class TypeSpec:
    """A C++ type under test.

    Attributes:
        tag: Short unique name, used to build algorithm and product names.
        annotation: Type annotation that the bindings map onto the C++ type.
        inputs: The two product suffixes that cppsource4py provides for this type.
        combine: Scalar binary operation used for the "scalar" output kind.
        np_dtype: Numpy dtype for array output, or None to skip numpy output.
        rel_tol: Relative tolerance used when verifying round-tripped values.
    """

    tag: str
    annotation: Any
    inputs: tuple[str, str]
    combine: Callable[[Any, Any], Any]
    np_dtype: Any = None
    rel_tol: float = 0.0

# fmt: off
TYPES = [
    #        tag       annotation       inputs          scalar operation             numpy dtype  rel_tol
    TypeSpec("bool",   bool,            ("b1", "b2"),   lambda a, b: a and b,        None),
    TypeSpec("int",    int,             ("i",  "j"),    lambda a, b: a + b,          np.int32),
    TypeSpec("uint",   "unsigned int",  ("u1", "u2"),   lambda a, b: a + b,          np.uint32),
    TypeSpec("long",   "long",          ("l1", "l2"),   lambda a, b: a + b,          np.int64),
    TypeSpec("ulong",  "unsigned long", ("ul1", "ul2"), lambda a, b: a + b,          np.uint64),
    TypeSpec("float",  float,           ("f1", "f2"),   lambda a, b: a + b,          np.float32,  1e-6),
    TypeSpec("double", "double",        ("d1", "d2"),   lambda a, b: float(a + b),   np.float64,  1e-12),
]
# fmt: on


# supported return kinds table
@dataclass(frozen=True)
class OutputKind:
    """Possible return types from Python to C++.

    Attributes:
        tag: Short unique name, used to build algorithm and product names.
        annotation: Maps a TypeSpec to the return annotation, or None if this
            output kind does not apply to that type.
        build: Computes the result from a TypeSpec and the two inputs.
    """

    tag: str
    annotation: Callable[[TypeSpec], Any]
    build: Callable[[TypeSpec, Any, Any], Any]


OUTPUT_KINDS = [
    OutputKind(
        "scalar",
        lambda t: t.annotation,
        lambda t, a, b: t.combine(a, b),
    ),
    OutputKind(
        "numpy",
        lambda t: None if t.np_dtype is None else npt.NDArray[t.np_dtype], # type: ignore # noqa: F821
        lambda t, a, b: np.array([a, b], dtype=t.np_dtype),
    ),
    OutputKind(
        "list",
        lambda t: None if t.np_dtype is None else list[t.annotation], # type: ignore # noqa: F821
        lambda t, a, b: [a, b],
    ),
]


def _test_cases():
    """Yield (spec, kind, return annotation, product name) for every valid combination."""
    for spec in TYPES:
        for kind in OUTPUT_KINDS:
            out = kind.annotation(spec)
            if out is not None:
                yield spec, kind, out, f"{kind.tag}_{spec.tag}"


def _check_unique_names():
    names = [p for *_, p in _test_cases()]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise ValueError(f"duplicate generated product names: {sorted(dupes)}")

_check_unique_names()


def _named(fn, name: str, annotations: dict, doc: str):
    """Give a generated function a unique name, annotations, and docstring."""
    fn.__name__ = fn.__qualname__ = name
    fn.__annotations__ = annotations
    fn.__doc__ = doc
    return fn


def _same_values(got, want, rel_tol: float) -> bool:
    """Compare scalars, numpy arrays, and lists element-wise."""
    got = np.atleast_1d(np.asarray(got)).tolist()
    want = np.atleast_1d(np.asarray(want)).tolist()
    return len(got) == len(want) and all(
        math.isclose(g, w, rel_tol=rel_tol, abs_tol=0.0)
        for g, w in zip(got, want, strict=True)
    )


def _make_transform(name, spec, kind, out_annotation):
    def transform(a, b):
        return kind.build(spec, a, b)

    return _named(
        transform,
        name,
        {"a": spec.annotation, "b": spec.annotation, "return": out_annotation},
        f"Combine two {spec.tag} inputs into a {kind.tag} output.",
    )


def _make_verify(name, spec, kind, out_annotation):
    def verify(a, b, result):
        expected = kind.build(spec, a, b)
        if not _same_values(result, expected, spec.rel_tol):
            raise AssertionError(
                f"{name}: inputs ({a!r}, {b!r}) gave {result!r}, expected {expected!r}"
            )

    return _named(
        verify,
        name,
        {"a": spec.annotation, "b": spec.annotation, "result": out_annotation},
        f"Verify the {kind.tag} output for {spec.tag} inputs.",
    )


def _input(config, suffix: str) -> dict:
    return {"creator": config["creator"], "layer": config["layer"], "suffix": suffix}


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register algorithms.

    For each combination of inputs and outputs, register both the transform
    that operates on the inputs and the observer that varifies them. All
    inputs come from a C++ producer.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """

    for spec, kind, out, product in _test_cases():
        inputs = [_input(config, s) for s in spec.inputs]
        m.transform(
            _make_transform(f"make_{product}", spec, kind, out),
            input_family=inputs,
            output_product_suffixes=[product],
        )
        m.observe(
            _make_verify(f"verify_{product}", spec, kind, out),
            input_family=[
                *inputs,
                {"creator": f"make_{product}", "layer": config["layer"], "suffix": product},
            ],
        )

"""A most basic provider.

This test code implements a simple provider to show that Python data
product can be injected into the execution graph and used by subsequent
algorithms.
"""

import numpy as np
import numpy.typing as npt

from phlex import Variant

_specs = (
    (-42, np.int32, "ii32"),
    (42, np.uint32, "iui32"),
    (-27, np.int64, "ii64"),
    (27, np.uint64, "iui64"),
    (42.0, np.float32, "if32"),
    (-42.0, np.float64, "if64"),
)

specs = [(False, np.bool_, "ib")]
for x, t, sf in _specs:
    specs.append((x, t, sf))  # type: ignore
    specs.append((np.array([x], dtype=t), npt.NDArray[t], "v" + sf))  # type: ignore


def PHLEX_REGISTER_PROVIDERS(s, config):
    """Register python providers for all supported types.

    Use the standard Phlex `provide` registration to insert nodes in the
    execution graph that receive a data call index and produces any of the
    supported types as output.

    Args:
        s (internal): Phlex source representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """

    def new_p(x):
        def p(di):
            assert 0 <= di.number()
            return x

        return p

    for x, t, sf in specs:
        f = Variant(new_p(x), {"di": "data_cell_index", "return": t}, "input_" + sf)
        s.provide(f, {"creator": "input_" + sf, "layer": "event", "suffix": sf})

    # add a bunch of failures to check error reporting
    try:
        s.provide(None)
        assert not "supposed to be here"
    except TypeError as e:
        assert "missing required argument" in str(e)

    try:

        class C:
            def __call__(self, di):
                pass

        s.provide(C(), {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except AttributeError as e:
        assert "__name__" in str(e)

    try:
        s.provide(42, {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except TypeError as e:
        assert "given provider is not callable" in str(e)

    try:
        s.provide(lambda: 42, {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except TypeError as e:
        assert "data_cell_index" in str(e)

    try:
        f = Variant(lambda di: 42, {"di": "data_cell_index", "return": None}, "f")
        s.provide(f, {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except TypeError as e:
        assert "provider must have an output" in str(e)

    try:
        f = Variant(lambda di: 42, {"di": "data_cell_index", "return": "object"}, "f")
        s.provide(f, {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except TypeError as e:
        assert "unsupported output type" in str(e)

    try:
        f = Variant(lambda di: 42, {"di": "data_cell_index", "return": npt.NDArray[np.bool_]}, "f")
        s.provide(f, {"creator": "a", "layer": "b", "suffix": "c"})
        assert not "supposed to be here"
    except TypeError as e:
        assert "unsupported collection output type" in str(e)


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register python consumers as observers to check providers' output.

    Use the standard Phlex `observe` registration to insert a node in the
    execution graph that receives an input from a python provider and
    verifies what it receives.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """

    def new_a(x):
        def a(y):
            assert y == x

        return a

    for x, t, sf in specs:
        f = Variant(new_a(x), {"y": t, "return": None}, sf + t.__name__)
        m.observe(f, input_family=[{"creator": "input_" + sf, "layer": "event"}])

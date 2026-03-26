"""A most basic provider.

This test code implements a simple provider to show that Python data
product can be injected into the execution graph and used by subsequent
algorithms.
"""

def provide_i(di: "data_cell_index") -> int:     # type: ignore # noqa: F821
    """Dummy 'i' source."""
    return di.number() % 2

def provide_j(di: "data_cell_index") -> int:     # type: ignore # noqa: F821
    """Dummmy 'j' source."""
    return 1 - di.number() % 2


def PHLEX_REGISTER_PROVIDERS(s, config):
    """Register the `provide` provider.

    Use the standard Phlex `provide` registration to insert a node
    in the execution graph that receives a data call index and produces
    an integer as an ouput.

    Args:
        s (internal): Phlex source representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    s.provide(provide_i, {"creator": "input", "layer": "event", "suffix": "i"})
    s.provide(provide_j, {"creator": "input", "layer": "event", "suffix": "j"})

"""
Convenience functions for registering Python algorithms with Phlex.
"""

_registered_algorithms = set()

def register(pyalg, m, config, concurrency=None):
    # inputs must exist
    inputs = config["input", str, True]

    # outputs are optional
    try:
        outputs = config["output", str, True]
    except TypeError:
        outputs = []

    m.register(pyalg, inputs, outputs, concurrency)

    # TODO: this ensures that both the algorithm and the associated module
    # are kept alive for the duration of the Phlex run. Depending on actual
    # usage, this may be overkill, but this seems to be the most convenient.
    _registered_algorithms.add(pyalg)

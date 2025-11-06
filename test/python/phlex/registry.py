"""
Convenience functions for registering Python algorithms with Phlex.
"""


def register(pyalg, m, config, concurrency=None):
    # inputs must exist
    inputs = config["input", str, True]

    # outputs are optional
    try:
        outputs = config["output", str, True]
    except TypeError:
        outputs = []

    m.register(pyalg, inputs, outputs, concurrency)


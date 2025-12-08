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

    if outputs:
        m.transform(pyalg, inputs, outputs, concurrency)
    else:
        m.observe(pyalg, inputs, outputs, concurrency)


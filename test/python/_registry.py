"""
Experimental Python-only registration based on full framework headers.
"""

_registered_modules = dict()

def register(m, config, concurrency=None):
    pymod_name = config["pymodule"]
    pyalg_name = config["pyalg"]

    # inputs must exist
    inputs = config["input", str, True]

    # outputs are optional
    try:
        outputs = config["output", str, True]
    except TypeError:
        outputs = []

    try:
        pymod = _registered_modules[pymod_name]
    except KeyError:
        pymod = __import__(pymod_name)
        _registered_modules[pymod_name] = pymod

    pyalg = getattr(pymod, pyalg_name)

    m.register(pyalg, inputs, outputs, concurrency)


"""Test that a malformed layer path in a selector raises ValueError."""


def identity(i: int) -> int:
    """Return the input unchanged."""
    return i


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register an algorithm whose selector has an invalid complete layer path."""
    try:
        m.transform(
            identity,
            input_family=[{"creator": "input", "layer": "/run/event", "suffix": "i"}],
            output_product_suffixes=["out"],
        )
    except ValueError as e:
        assert "must start with '/job'" in str(e)
    else:
        raise AssertionError("a layer path that does not start with '/job' was accepted")

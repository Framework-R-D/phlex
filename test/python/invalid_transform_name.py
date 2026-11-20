"""A transform with a name that is not a valid identifier.

Registration must fail with a ValueError that quotes the name exactly as written.
"""


def constant_one(i: int) -> int:
    """Constant 1."""
    return 1


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register a transform whose name is malformed.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    m.transform(
        constant_one,
        name="bad name",
        input_family=[{"creator": "input", "layer": "event", "suffix": "i"}],
        output_product_suffixes=["output_one"],
    )

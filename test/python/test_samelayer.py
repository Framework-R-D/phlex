"""Test that selectors naming one layer by different path suffixes are accepted."""


def in_a(di: "data_cell_index") -> int:  # type: ignore[name-defined]  # noqa: F821
    """Provide the first value."""
    return 2


def in_b(di: "data_cell_index") -> int:  # type: ignore[name-defined]  # noqa: F821
    """Provide the second value."""
    return 3


def add(a: int, b: int) -> int:
    """Add two integers."""
    return a + b


def check(total: int) -> None:
    """Check the sum of the two provided values."""
    assert total == 5


def PHLEX_REGISTER_PROVIDERS(s, config):
    """Provide two integers in the complete layer path /job/event."""
    s.provide(in_a, {"creator": "in_a", "layer": "/job/event", "suffix": "a"})
    s.provide(in_b, {"creator": "in_b", "layer": "/job/event", "suffix": "b"})


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register a transform whose two input layers are suffixes of one another."""
    m.transform(
        add,
        input_family=[
            {"creator": "in_a", "layer": "event", "suffix": "a"},
            {"creator": "in_b", "layer": "/job/event", "suffix": "b"},
        ],
        output_product_suffixes=["sum"],
    )
    m.observe(check, input_family=[{"creator": "add", "layer": "event", "suffix": "sum"}])

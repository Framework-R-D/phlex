"""An observer to check for output in tests.

Test algorithms produce outputs. To ensure that a test is run correctly,
this observer verifies its result against the expected value.
"""

from typing import Any


class Verifier:
    """A callable class that can assert an expected value.

    Attributes:
        __name__ (str): Identifier for Phlex.

    Examples:
        >>> v = Verifier(42)
        >>> v(42)
        >>> v(21)
        Traceback (most recent call last):
          ...
        AssertionError
    """

    __name__ = "verifier"

    def __init__(self, expected_value: Any):
        """Create a verifier object.

        Args:
            expected_value (Any): The expected value.
        """
        self._expected_value = expected_value

    def __call__(self, value: int) -> None:
        """Verify the `value`.

        Check that `value` matches the pre-registered value.

        Args:
            value (int): The value to verify.

        Raises:
            AssertionError: if the provided value does not match the
                pre-registered value.
        """
        assert value == self._expected_value


class BoolVerifier:
    """Verifier for boolean values."""

    __name__ = "bool_verifier"

    def __init__(self, expected: bool):
        """Create a boolean verifier."""
        self._expected = expected

    def __call__(self, value: bool) -> None:
        """Verify the boolean value."""
        assert value == self._expected


def PHLEX_REGISTER_ALGORITHMS(m, config):
    """Register an instance of `Verifier` as an observer.

    Use the standard Phlex `observe` registration to insert a node in
    the execution graph that receives a value to check against an
    expected value. The expected value is taken from the configuration.

    Args:
        m (internal): Phlex registrar representation.
        config (internal): Phlex configuration representation.

    Returns:
        None
    """
    try:
        expected = config["expected_bool"]
        v = BoolVerifier(expected)
        m.observe(v, input_family=config["input"])
        return
    except KeyError:
        pass

    assert_sum = Verifier(config["sum_total"])
    m.observe(assert_sum, input_family=config["input"])

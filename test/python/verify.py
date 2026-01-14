"""An observer to check for output in tests.

Test algorithms produce outputs. To ensure that a test is run correctly,
this observer verifies its result against the expected value.
"""


class Verifier:
    """A callable class that can assert an expected value.

    Attributes:
        __name__ (str): Identifier for Phlex.

    Examples:
        >>> v = Verifier(42)
        >>> v.(42)
        >>> v.(21)
        Traceback (most recent call last):
          File "<stdin>", line 1, in <module>
          File "verify.py", line 22, in __call__
            assert value == self._sum_total
                   ^^^^^^^^^^^^^^^^^^^^^^^^
        AssertionError
    """

    __name__ = "verifier"

    def __init__(self, sum_total: int):
        """Create a verifier object.

        Args:
            sum_total (int): The expected value.

        Returns:
            None
        """
        self._sum_total = sum_total

    def __call__(self, value: int) -> None:
        """Verify a the `value`.

        Check that `value` matches the pre-registered value.

        Args:
            value (int): The value to verify.

        Raises:
            AssertionError: if the provided value does not matches the
                pre-registed value.

        Returns:
            None
        """
        assert value == self._sum_total


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
    the execution graph that receives a summed total to check against an
    expected value. The expected total is taken from the configuration.

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
    except Exception:
        pass

    assert_sum = Verifier(config["sum_total"])
    m.observe(assert_sum, input_family=config["input"])

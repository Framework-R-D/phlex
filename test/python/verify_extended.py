"""Observers to check for various types in tests."""

import sys


class VerifierInt:
    """Verify int values."""

    __name__ = "verifier_int"

    def __init__(self, sum_total: int):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: int) -> None:
        """Check if value matches expected sum."""
        assert value == self._sum_total


class VerifierUInt:
    """Verify unsigned int values."""

    __name__ = "verifier_uint"

    def __init__(self, sum_total: int):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: "unsigned int") -> None:  # type: ignore # noqa: F722
        """Check if value matches expected sum."""
        assert value == self._sum_total


class VerifierLong:
    """Verify long values."""

    __name__ = "verifier_long"

    def __init__(self, sum_total: int):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: "long") -> None:  # type: ignore # noqa: F821
        """Check if value matches expected sum."""
        print(f"VerifierLong: value={value}, expected={self._sum_total}")
        assert value == self._sum_total


class VerifierULong:
    """Verify unsigned long values."""

    __name__ = "verifier_ulong"

    def __init__(self, sum_total: int):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: "unsigned long") -> None:  # type: ignore # noqa: F722
        """Check if value matches expected sum."""
        print(f"VerifierULong: value={value}, expected={self._sum_total}")
        assert value == self._sum_total


class VerifierFloat:
    """Verify float values."""

    __name__ = "verifier_float"

    def __init__(self, sum_total: float):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: "float") -> None:
        """Check if value matches expected sum."""
        sys.stderr.write(f"VerifierFloat: value={value}, expected={self._sum_total}\n")
        assert abs(value - self._sum_total) < 1e-5


class VerifierDouble:
    """Verify double values."""

    __name__ = "verifier_double"

    def __init__(self, sum_total: float):
        """Initialize with expected sum."""
        self._sum_total = sum_total

    def __call__(self, value: "double") -> None:  # type: ignore # noqa: F821
        """Check if value matches expected sum."""
        print(f"VerifierDouble: value={value}, expected={self._sum_total}")
        assert abs(value - self._sum_total) < 1e-5


class VerifierBool:
    """Verify bool values."""

    __name__ = "verifier_bool"

    def __init__(self, expected: bool):
        """Initialize with expected value."""
        self._expected = expected

    def __call__(self, value: bool) -> None:
        """Check if value matches expected."""
        print(f"VerifierBool: value={value}, expected={self._expected}")
        assert value == self._expected


def PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config):
    """Register observers for the test."""
    try:
        m.observe(VerifierInt(config["sum_total"]), input_family=config["input_int"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierBool(config["expected_bool"]), input_family=config["input_bool"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierUInt(config["sum_total"]), input_family=config["input_uint"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierLong(config["sum_total"]), input_family=config["input_long"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierULong(config["sum_total"]), input_family=config["input_ulong"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierFloat(config["sum_total"]), input_family=config["input_float"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

    try:
        m.observe(VerifierDouble(config["sum_total"]), input_family=config["input_double"])
    except (KeyError, TypeError):
        # Optional configuration, skip if missing
        pass

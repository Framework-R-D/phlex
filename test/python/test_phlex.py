"""This module contains tests for the pyphlex module."""


class TestPYPHLEX:
    """Tests for the pyphlex module."""

    @classmethod
    def setup_class(cls):
        """Set up the test class."""
        import phlexpy  # noqa: F401  # type: ignore[import-not-found]

        __all__ = ["phlexpy"]  # noqa: F841  # For CodeQL

    def test01_phlex_existence(self):
        """Test existence of the phlex namespace."""
        import cppyy  # type: ignore[import-not-found]

        assert cppyy.gbl.phlex

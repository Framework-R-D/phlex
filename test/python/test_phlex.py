"""This module contains tests for the pyphlex module."""


class TestPYPHLEX:
    """Tests for the pyphlex module."""

    @classmethod
    def setup_class(cls):
        """Set up the test class."""
        import pyphlex  # noqa: F401

        __all__ = ["pyphlex"]  # noqa: F841  # For CodeQL

    def test01_phlex_existence(self):
        """Test existence of the phlex namespace."""
        import cppyy

        assert cppyy.gbl.phlex

import py
from pytest import raises


class TestPYPHLEX:

    def setup_class(cls):
        import pyphlex

    def test01_phlex_existence(self):
        """Test existence of the phlex namespace"""

        import cppyy

        assert cppyy.gbl.phlex

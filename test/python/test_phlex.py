

class TestPYPHLEX:
    def setup_class(cls):
        pass

    def test01_phlex_existence(self):
        """Test existence of the phlex namespace"""

        import cppyy

        assert cppyy.gbl.phlex
